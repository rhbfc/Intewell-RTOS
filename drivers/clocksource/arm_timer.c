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

/************************头 文 件******************************/
#include "arch_helpers.h"
#include <arch_timer.h>
#include <barrier.h>
#include <time/ktime.h>
#include <commonUtils.h>
#include <cpuid.h>
#include <ttos_pic.h>
#include <ttos_time.h>
#include <inttypes.h>
#include <time/kitimer.h>

#define KLOG_TAG "ARM Timer"
#include <klog.h>

/************************宏 定 义******************************/
#define CNTKCTL_PTEN  (1UL << 9)
#define CNTKCTL_VTEN  (1UL << 8)
#define CNTKCTL_VCTEN (1UL << 1)
#define CNTKCTL_PCTEN (1UL << 0)

#define CNTX_CTL_ENABLE (1UL << 0)
#define CNTX_CTL_IMASK  (1UL << 1)

/************************类型定义******************************/
/************************外部声明******************************/
uint64_t ns_to_timer_count (uint64_t ns, uint64_t freq);
int arch_timer_freq_init(void);

/************************前向声明******************************/
static s32 arm_timer_init (void);
static s32 arm_timer_enable (void);
static s32 arm_timer_disable (void);
static u64 arm_timer_count_get (void);
static s32 arm_timer_count_set (u64 count);
static u64 arm_timer_freq_get (void);
static u64 arm_timer_walltime_get (void);
static s32 arm_timer_timeout_set (u64 timeout);
static s32 arm_timer_timeout_ns_set (u64 ns);

/************************模块变量******************************/
static ttos_time_ops_t arm_timer_ops
    = { .time_name         = "ARM Gen Timer",
        .time_init         = arm_timer_init,
        .time_enable       = arm_timer_enable,
        .time_disable      = arm_timer_disable,
        .time_count_get    = arm_timer_count_get,
        .time_count_set    = arm_timer_count_set,
        .time_freq_get     = arm_timer_freq_get,
        .time_walltime_get = arm_timer_walltime_get,
        .time_timeout_set  = arm_timer_timeout_set,
        .time_timeout_ns_set = arm_timer_timeout_ns_set
        };

/************************全局变量******************************/
/************************实   现*******************************/

/************************外部实现start******************************/

/************************外部实现end******************************/
/**
 * @brief
 *    timer配置
 * @param[in] freq 1s产生的tick数，即1s产生freq次中断
 * @retval 0 设置成功
 */
static s32 arm_timer_config (u64 freq)
{
    u64 val = 0;

    /*配置EL0访问物理定时器、虚拟定时器、频率寄存器、counter寄存器陷入*/
    val = arch_timer_cntkctl_read ();
    val &= ~(CNTKCTL_PTEN | CNTKCTL_VTEN | CNTKCTL_PCTEN);
    val |= (CNTKCTL_VCTEN|CNTKCTL_PCTEN);
    arch_timer_cntkctl_write (val);

    /*根据freq设置downcounter寄存器*/
    val = arch_timer_cntfrq_read ();
    arch_timer_cntp_tval_write (val / freq);

    isb ();

    return (0);
}

/**
 * @brief
 *    time 初始化
 * @param[in] 无
 * @retval EIO 失败
 * @retval 0 成功
 */
static s32 arm_timer_init (void)
{
    /*timer配置*/
    arm_timer_config (1);

    if (is_bootcpu ())
    {
        ktimer_set_resolution(ttos_time_freq_get());
        KLOG_I ("arm timer freq:%" PRIu64, arm_timer_freq_get ());
    }

    return (0);
}

/**
 * @brief
 *    使能timer
 * @param[in] 无
 * @retval 0 设置成功
 */
static s32 arm_timer_enable (void)
{
    /*使能timer*/
    arch_timer_cntp_ctl_write (CNTX_CTL_ENABLE);
    isb ();

    return (0);
}

/**
 * @brief
 *    关闭timer
 * @param[in] 无
 * @retval 0 设置成功
 */
static s32 arm_timer_disable (void)
{
    /*disable timer*/
    arch_timer_cntp_ctl_write (0);
    isb ();

    return (0);
}

/**
 * @brief
 *    获取timer count
 * @param[in] 无
 * @retval  time count
 */
static u64 arm_timer_count_get (void)
{
    isb ();

    return arch_timer_cntpct_read ();
}

/**
 * @brief
 *    设置timer count
 * @param[in] count time count
 * @retval 0 设置成功
 */
static s32 arm_timer_count_set (u64 count)
{
    arch_timer_cntp_cval_write (count);

    /* 使能timer */
    arm_timer_enable ();

    return (0);
}

/**
 * @brief
 *    获取timer freq
 * @param[in] 无
 * @retval  time freq
 */
static u64 arm_timer_freq_get (void)
{
    return arch_timer_cntfrq_read ();
}

/**
 * @brief
 *    获取timer墙上时间
 * @param[in] 无
 * @retval 墙上时间，nanoseconds
 */
static u64 arm_timer_walltime_get (void)
{
    u64 freq, count;
    u64 seconds, nanoseconds, walltime;

    /*获取频率*/
    freq = arm_timer_freq_get ();

    /*获取timer count*/
    count = arm_timer_count_get ();

    /*计算walltime*/
    seconds     = count / freq;
    nanoseconds = ((count % freq) * NSEC_PER_SEC) / freq;
    walltime    = seconds * NSEC_PER_SEC + nanoseconds;

    return walltime;
}

/**
 * @brief
 *    设置timer超时时刻
 * @param[in] timeout 超时时刻，nanoseconds
 * @retval EIO 失败
 * @retval 0 成功
 */
static s32 arm_timer_timeout_set (u64 timeout)
{
    u64 freq, count;
    u64 seconds;

    /*获取频率*/
    freq = arm_timer_freq_get ();

    /*计算count*/
    seconds = timeout / NSEC_PER_SEC;
    count   = ((timeout % NSEC_PER_SEC) * freq) / NSEC_PER_SEC;
    count   = seconds * freq + count;

    return arm_timer_count_set (count);
}

static s32 arm_timer_timeout_ns_set (u64 ns)
{
    u64 freq = arm_timer_freq_get ();
    uint64_t count = ns_to_timer_count (ns, freq);
    return arm_timer_count_set (count);
}
/**
 * @brief
 *    arm timer初始化，注册ttos_timer操作函数
 * @param[in] 无
 * @retval 0 初始化成功
 */
s32 arch_timer_pre_init (void)
{
    if (is_bootcpu ())
    {
        /*注册arm timer操作函数*/
        ttos_time_register (&arm_timer_ops);

        arch_timer_freq_init ();
    }

    return (0);
}
