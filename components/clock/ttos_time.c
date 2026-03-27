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
#include <time/ktime.h>
#include <commonUtils.h>
#include <errno.h>
#include <stdlib.h>
#include <ttos_time.h>

#define KLOG_TAG "Time"
#include <klog.h>

/************************宏 定 义******************************/
/************************类型定义******************************/
/************************外部声明******************************/
/************************前向声明******************************/
/************************模块变量******************************/
static ttos_time_ops_t *time_ops         = NULL;
static time_handler_t   time_handler = NULL;

/************************全局变量******************************/
/************************实   现*******************************/

/**
 * @brief
 *    注册time DRV
 * @param[in] ops ttos_timer_ops_t结构指针
 * @retval 0 设置成功
 * @retval EIO 设置失败
 */
s32 ttos_time_register (ttos_time_ops_t *ops)
{
    if (!ops)
    {
        return (-EIO);
    }

    time_ops = ops;

    return (0);
}

/**
 * @brief
 *    卸载time DRV
 * @param[in] 无
 * @retval 无
 */
void ttos_time_unregister (void)
{
    time_ops = NULL;
}

/**
 * @brief
 *    注册time中断handler
 * @param[in] handler time中断handler
 * @retval 0 成功
 * @retval EIO 失败
 */
s32 ttos_time_handler_register (time_handler_t handler)
{
    if (!handler)
    {
        return (-EIO);
    }

    time_handler = handler;

    return (0);
}

/**
 * @brief
 *    time中断handler
 * @param[in] 无
 * @retval 无
 */
void ttos_time_handler (void)
{
    if (time_handler)
    {
        time_handler ();
    }
}

/**
 * @brief
 *    time初始化
 * @param[in] 无
 * @retval 0 成功
 * @retval EIO 失败
 */
s32 ttos_time_init (void)
{
    if (!time_ops || !time_ops->time_init)
    {
        return (-EIO);
    }

    return time_ops->time_init ();
}

/**
 * @brief
 *    关闭time
 * @param[in] 无
 * @retval 0 设置成功
 * @retval EIO 设置失败
 */
s32 ttos_time_disable (void)
{
    if (!time_ops || !time_ops->time_disable)
    {
        return (-EIO);
    }

    return time_ops->time_disable ();
}

/**
 * @brief
 *    打开time
 * @param[in] 无
 * @retval 0 设置成功
 * @retval EIO 设置失败
 */
s32 ttos_time_enable (void)
{
    if (!time_ops || !time_ops->time_enable)
    {
        return (-EIO);
    }

    return time_ops->time_enable ();
}

/**
 * @brief
 *    获取time count
 * @param[in] 无
 * @retval  time count
 * @retval  0 空指针
 */
u64 ttos_time_count_get (void)
{
    if (!time_ops || !time_ops->time_count_get)
    {
        return (0);
    }

    return time_ops->time_count_get ();
}

/**
 * @brief
 *    设置time count
 * @param[in] count time count
 * @retval 0 设置成功
 */
s32 ttos_time_count_set (u64 count)
{
    if (!time_ops || !time_ops->time_count_set)
    {
        return (-EIO);
    }

    return time_ops->time_count_set (count);
}

/**
 * @brief
 *    获取time freq
 * @param[in] 无
 * @retval  time freq
 * @retval  0 空指针
 */
u64 ttos_time_freq_get (void)
{
    if (!time_ops || !time_ops->time_freq_get)
    {
        return (0);
    }

    return time_ops->time_freq_get ();
}

/**
 * @brief
 *    获取time墙上时间
 * @param[in] 无
 * @retval  墙上时间，nanoseconds
 * @retval  0 空指针
 */
u64 ttos_time_walltime_get (void)
{
    if (!time_ops || !time_ops->time_walltime_get)
    {
        return (0);
    }

    return time_ops->time_walltime_get ();
}

/**
 * @brief
 *    设置time超时时刻
 * @param[in] timeout 超时时刻，nanoseconds
 * @retval EIO 失败
 * @retval 0 成功
 */
s32 ttos_time_timeout_set (u64 timeout)
{
    if (!time_ops || !time_ops->time_timeout_set)
    {
        return (0);
    }

    return time_ops->time_timeout_set (timeout);
}

s32 ttos_time_timeout_ns_set (u64 ns)
{
    if (!time_ops || !time_ops->time_timeout_ns_set)
    {
        return (0);
    }

    return time_ops->time_timeout_ns_set(ns);
}

/**
 * @brief
 *    将count转换为ns
 * @param[in] count time count
 * @retval 墙上时间，nanoseconds
 */
u64 ttos_time_count_to_ns (u64 count)
{
    u64 freq;
    u64 seconds, nanoseconds;

    /*获取频率*/
    freq = ttos_time_freq_get ();

    if (freq == 0)
    {
        KLOG_E ("ttos time freq error at %s %d", __func__, __LINE__);
        return count;
    }

    /*计算nanoseconds*/
    seconds     = count / freq;
    nanoseconds = ((count % freq) * NSEC_PER_SEC) / freq;
    nanoseconds = seconds * NSEC_PER_SEC + nanoseconds;

    return nanoseconds;
}

/*
 * @brief:
 *    延迟指定的时间，单位微秒(ns)
 * @param[in] ns 延迟的纳秒数
 * @retval 无
 */
void ttos_time_ndelay (u64 ns)
{
    u64 end_time = ttos_time_walltime_get () + ns;
    u64 now_time = 0;

    do
    {
        now_time = ttos_time_walltime_get ();
    } while (now_time < end_time);
}

/*
 * @brief:
 *    延迟指定的时间，单位微秒(us)
 * @param[in] us 延迟的微秒数
 * @retval 无
 */
void ttos_time_udelay (u64 us)
{
    u64 end_time = ttos_time_walltime_get () + us * NSEC_PER_USEC;
    u64 now_time = 0;

    do
    {
        now_time = ttos_time_walltime_get ();
    } while (now_time < end_time);
}

/*
 * @brief:
 *    延迟指定的时间，单位毫秒(ms)
 * @param[in] ms 延迟的毫秒数
 * @retval 无
 */
void ttos_time_mdelay (u64 ms)
{
    ttos_time_udelay (ms * USEC_PER_MSEC);
}
