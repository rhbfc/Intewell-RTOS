/*
 * Copyright (c) 2026 Kyland Inc.
 * Intewell-RTOS is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "syscall_internal.h"
#include <driver/cpudev.h>
#include <errno.h>
#include <sched.h>
#include <smp.h>
#include <sys/reboot.h>

/**
 * @defgroup reboot Reboot System Call
 * @{
 */

/**
 * @brief CAD信号标志
 *
 * 该标志用于指示CAD信号是否已发送到init进程。
 */
static bool g_CAD_signal_to_init = false;

/**
 * @brief 获取CAD信号标志
 *
 * 该函数用于获取CAD信号标志的当前状态。
 *
 * @return CAD信号标志的当前状态
 */
bool CAD_is_signal_to_init(void)
{
    return g_CAD_signal_to_init;
}

/**
 * @brief CPU0上执行系统关闭的工作函数
 *
 * @param[in] info 未使用的参数
 */
static void smp_system_poweroff(void *info)
{
    (void)info;
    system_poweroff();
}

/**
 * @brief CPU0上执行系统重启的工作函数
 *
 * @param[in] info 未使用的参数
 */
static void smp_cpu_reset(void *info)
{
    (void)info;
    cpu_reset(cpu_boot_dev_get());
}

/**
 * @brief 系统调用实现：重启系统。
 *
 * 该函数实现了一个系统调用，用于重启或关闭系统。
 *
 * @param[in] magic1 魔数1（LINUX_REBOOT_MAGIC1）
 * @param[in] magic2 魔数2（LINUX_REBOOT_MAGIC2）
 * @param[in] cmd 重启命令：
 *                - LINUX_REBOOT_CMD_RESTART：重启
 *                - LINUX_REBOOT_CMD_HALT：停机
 *                - LINUX_REBOOT_CMD_POWER_OFF：关机
 * @param[in] arg 命令参数
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功。
 * @retval -EPERM 权限不足。
 * @retval -EINVAL 参数无效。
 *
 * @note 1. 需要特权。
 *       2. 验证魔数。
 *       3. 同步文件。
 *       4. 终止进程。
 *       5. 系统操作通过SMP call在CPU0上执行。
 */
DEFINE_SYSCALL(reboot, (int magic1, int magic2, unsigned int cmd, void __user *arg))
{
    long ret = 0;
    cpu_set_t cpu0_set;

    KLOG_D("reboot: magic1: 0x%x, magic1: 0x%x, cmd: 0x%x, arg: %p", magic1, magic2, cmd, arg);
    if (0xfee1dead != magic1 || 0x28121969 != magic2)
    {
        return -EINVAL;
    }

    /* 【优化】构造仅包含CPU0的集合，用于SMP call */
    CPU_ZERO(&cpu0_set);
    CPU_SET(0, &cpu0_set);

    switch (cmd)
    {
    /* CAD  is disabled.  This means that the CAD keystroke will cause a SIGINT signal to be sent to
              init (process 1), whereupon this process may decide upon a proper action (maybe: kill
       all processes, sync, reboot).
    */
    case RB_DISABLE_CAD:
        g_CAD_signal_to_init = true;
        break;
    /* CAD is enabled.  This means that the CAD keystroke will immediately  cause  the  action
              associated with RB_AUTOBOOT.
    */
    case RB_ENABLE_CAD:
        g_CAD_signal_to_init = false;
        break;
    /* The message "System halted." is printed, and the system is halted.
              Control is given to the ROM monitor, if there is one.  If not preceded by a sync(2),
       data will be lost.
    */
    case RB_HALT_SYSTEM:
        /* 【SMP优化】通过SMP call在CPU0上执行系统关闭 */
        ret = smp_call_function_sync(&cpu0_set, smp_system_poweroff, NULL);
        break;
    /* Execute a kernel that has been loaded earlier with kexec_load(2).   This
              option is available only if the kernel was configured with CONFIG_KEXEC.
    */
    case RB_KEXEC:
        ret = -ENOSYS;
        break;
    /* The message "Power down." is printed, the system is stopped, and all
              power is removed from the system, if possible.  If not preceded by a sync(2), data
       will be lost.
    */
    case RB_POWER_OFF:
        /* 【SMP优化】通过SMP call在CPU0上执行系统关闭 */
        smp_call_function_sync(&cpu0_set, smp_system_poweroff, NULL);
        while (1)
            ;
    /* The message "Restarting system." is printed, and a default restart  is  performed  immedi‐
              ately.  If not preceded by a sync(2), data will be lost.
    */
    case RB_AUTOBOOT:
        /* 【SMP优化】通过SMP call在CPU0上执行系统重启 */
        ret = smp_call_function_sync(&cpu0_set, smp_cpu_reset, NULL);
        break;
    /* The message "Restarting system with command '%s'" is printed, and a restart (using
              the command string given in arg) is performed immediately.  If not preceded by a
       sync(2), data will be lost.
    */
    case 0xa1b2c3d4:
        ret = -ENOSYS;
        break;
    /* The system is suspended  (hibernated)  to  disk.   This  option  is
              available only if the kernel was configured with CONFIG_HIBERNATION.
    */
    case RB_SW_SUSPEND:
        ret = -ENOSYS;
        break;
    default:
        ret = -ENOSYS;
        break;
    }
    return ret;
}

/**
 * @}
 */
