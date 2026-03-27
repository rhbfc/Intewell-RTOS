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
#include <sys/prctl.h>
#include <ttosProcess.h>
#include <uaccess.h>

/**
 * @brief 系统调用实现：进程控制操作。
 *
 * 该函数实现了一个系统调用，用于控制进程的各种属性。
 *
 * @param[in] option 控制选项：
 *                   - PR_SET_NAME：设置进程名
 *                   - PR_GET_NAME：获取进程名
 *                   - PR_SET_PDEATHSIG：设置父进程终止信号
 *                   - PR_GET_PDEATHSIG：获取父进程终止信号
 * @param[in] arg2 选项参数2
 * @param[in] arg3 选项参数3
 * @param[in] arg4 选项参数4
 * @param[in] arg5 选项参数5
 * @return 成功时返回0或正值，失败时返回负值错误码。
 * @retval 0 成功。
 * @retval >0 成功（某些选项）。
 * @retval -EINVAL 参数无效。
 * @retval -EFAULT 内存访问错误。
 *
 * @note 1. 选项参数含义依赖于option。
 *       2. 部分选项需要特权。
 *       3. 影响进程属性。
 *       4. 部分设置可继承。
 */
DEFINE_SYSCALL(prctl, (int option, unsigned long arg2, unsigned long arg3, unsigned long arg4,
                       unsigned long arg5))
{
    int ret = -ENOSYS;
    pcb_t pcb = ttosProcessSelf();
    switch (option)
    {
    case PR_CAP_AMBIENT:
        switch (arg2)
        {
        case PR_CAP_AMBIENT_RAISE:
        case PR_CAP_AMBIENT_LOWER:
        case PR_CAP_AMBIENT_IS_SET:
        case PR_CAP_AMBIENT_CLEAR_ALL:

        default:
            break;
        }
        break;
    case PR_CAPBSET_READ:
        break;
    case PR_CAPBSET_DROP:
        break;
    /* 子重载进程 防止僵尸进程产生 本来就是 */
    case PR_SET_CHILD_SUBREAPER:
        if (!arg2)
        {
            ret = -ENOSYS;
            break;
        }
        ret = 0;
        break;
    case PR_GET_CHILD_SUBREAPER:
    {
        int status = 1;
        ret = copy_to_user((void *)arg2, &status, sizeof(int));
        break;
    }
    /* PowerPC only */
    case PR_SET_ENDIAN:
        ret = -ENOSYS;
        break;
    /* PowerPC only */
    case PR_GET_ENDIAN:
        ret = -ENOSYS;
        break;
    /* only on MIPS */
    case PR_SET_FP_MODE:
        ret = -ENOSYS;
        break;
    /* only on MIPS */
    case PR_GET_FP_MODE:
        ret = -ENOSYS;
        break;
    /* only on ia64 */
    case PR_SET_FPEMU:
        ret = -ENOSYS;
        break;
    /* only on ia64 */
    case PR_GET_FPEMU:
        ret = -ENOSYS;
        break;

    /* only on PowerPC */
    case PR_SET_FPEXC:
        ret = -ENOSYS;
        break;
    /* only on PowerPC */
    case PR_GET_FPEXC:
        ret = -ENOSYS;
        break;

    /* IO刷新器 */
    case PR_SET_IO_FLUSHER:
        ret = -ENOSYS;
        break;
    case PR_GET_IO_FLUSHER:
        ret = -ENOSYS;
        break;

    case PR_SET_KEEPCAPS:
        ret = -ENOSYS;
        break;
    case PR_GET_KEEPCAPS:
        ret = -ENOSYS;
        break;

    /* 设置 MCE 处理策略 */
    case PR_MCE_KILL:
        ret = -ENOSYS;
        break;
    case PR_MCE_KILL_GET:
        ret = -ENOSYS;
        break;
    /* MMU 不支持用户操作 */
    case PR_SET_MM:
        ret = -ENOSYS;
        break;

    /* removed in Linux 5.4 */
    case PR_MPX_ENABLE_MANAGEMENT:
        ret = -ENOSYS;
        break;
    case PR_MPX_DISABLE_MANAGEMENT:
        ret = -ENOSYS;
        break;
    case PR_SET_NAME:
    {
        ret = strcpy_from_user(pcb->cmd_name, (char *)arg2);
        if (ret > 0)
        {
            ret = 0;
        }
        break;
    }
    case PR_GET_NAME:
    {
        ret = strncpy_to_user((char *)arg2, pcb->cmd_name, 16);
        if (ret > 0)
        {
            ret = 0;
        }
        break;
    }
    case PR_SET_NO_NEW_PRIVS:
        ret = -ENOSYS;
        break;
    case PR_GET_NO_NEW_PRIVS:
        ret = -ENOSYS;
        break;
    /* (since Linux 5.0, only on arm64) */
    case PR_PAC_RESET_KEYS:
        ret = -ENOSYS;
        break;
    /* 设置父进程死亡信号: 当调用 prctl(PR_SET_PDEATHSIG, signum) 时，
    可以指定一个信号（signum），当父进程（即调用 prctl 的进程）终止时，
    子进程将收到该信号。这可以用于通知子进程父进程已经结束。*/
    case PR_SET_PDEATHSIG:
        ret = -ENOSYS;
        break;
    case PR_GET_PDEATHSIG:
        ret = -ENOSYS;
        break;
    case PR_SET_SECCOMP:
        ret = -ENOSYS;
        break;

    case PR_GET_SECCOMP:
        ret = -ENOSYS;
        break;
    case PR_SET_SECUREBITS:
        ret = -ENOSYS;
        break;
    case PR_GET_SECUREBITS:
        ret = -ENOSYS;
        break;
    case PR_GET_SPECULATION_CTRL:
        ret = -ENOSYS;
        break;
    case PR_SET_SPECULATION_CTRL:
        ret = -ENOSYS;
        break;
    case PR_SVE_SET_VL:
        ret = -ENOSYS;
        break;
    case PR_SVE_GET_VL:
        ret = -ENOSYS;
        break;
    /* (since Linux 5.4, only on arm64) */
    case PR_SET_TAGGED_ADDR_CTRL:
        ret = -ENOSYS;
        break;
    case PR_GET_TAGGED_ADDR_CTRL:
        ret = -ENOSYS;
        break;
    /* 性能计数器 */
    case PR_TASK_PERF_EVENTS_DISABLE:
        ret = -ENOSYS;
        break;
    case PR_TASK_PERF_EVENTS_ENABLE:
        ret = -ENOSYS;
        break;

    case PR_SET_THP_DISABLE:
        ret = -ENOSYS;
        break;
    case PR_GET_THP_DISABLE:
        ret = -ENOSYS;
        break;

    case PR_GET_TID_ADDRESS:
        ret = copy_to_user((void *)arg2, &pcb->clear_child_tid, sizeof(int *));
        break;
    /* 设置定时器松弛: 调用 prctl(PR_SET_TIMERSLACK, slack)
    可以为当前进程设置一个新的定时器松弛值，
    其中 slack 是一个以微秒为单位的整数，
    表示在定时器到期前的最大时间延迟。
    通过允许一定的松弛，系统可以更有效地调度任务。 */
    case PR_SET_TIMERSLACK:
        ret = -ENOSYS;
        break;
    case PR_GET_TIMERSLACK:
        ret = -ENOSYS;
        break;
    /* 获取定时器信息: 通过调用 prctl(PR_GET_TIMING, ...)，
    可以获取当前进程的定时器设置，
    包括与定时器行为相关的参数。
    这可以帮助开发者分析和优化进程的定时器使用情况。 */
    case PR_SET_TIMING:
        ret = -ENOSYS;
        break;
    case PR_GET_TIMING:
        ret = -ENOSYS;
        break;
    /* 设置 TSC 行为: 通过调用 prctl(PR_SET_TSC, flag)，
    可以控制当前进程对 TSC 的使用行为。
    具体的 flag 值可以指定如何处理 TSC，
    例如启用或禁用某些功能。 */
    case PR_SET_TSC:
        ret = -ENOSYS;
        break;
    case PR_GET_TSC:
        ret = -ENOSYS;
        break;
    case PR_SET_UNALIGN:
        ret = -ENOSYS;
        break;
    /* 获取非对齐访问行为: 通过调用 prctl(PR_GET_UNALIGN)，
    可以获取当前进程的非对齐访问设置。
    这通常用于检测进程是否允许非对齐访问，
    以及系统如何处理这些访问。*/
    case PR_GET_UNALIGN:
        ret = -ENOSYS;
        break;
    default:
        break;
    }

    if (ret == -ENOSYS)
    {
        KLOG_E("prctl not support option %d\n", option);
    }
    return ret;
}
