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

#include <time/ktime.h>
#include <ttos.h>

/**
 * @brief 将时间转为时钟滴答数，以相对时间表示
 *
 * 该函数将时间转为时钟滴答数，以相对时间表示
 *
 * @param[in] time timespec格式表示的时间
 * @return 成功时返回tick，失败时返回TTOS_WAIT_FOREVER
 * @retval tick  成功。
 * @retval TTOS_WAIT_FOREVER 失败。
 */
clock_t timespec64_to_clock_t(const struct timespec64 *time)
{
    if (time == NULL)
    {
        return TTOS_WAIT_FOREVER;
    }

    struct timespec t = {time->tv_sec, time->tv_nsec};

    return (clock_t)clock_time_to_tick(&t, false);
}

/**
 * @brief 将时间转为时钟滴答数
 *
 * 该函数将时间转为时钟滴答数
 *
 * @param[in] time timespec格式表示的时间
 * @param[in] is_abs_timeout 是否时绝对时间
 * @return 成功时返回tick，失败时返回TTOS_WAIT_FOREVER
 * @retval tick  成功。
 * @retval TTOS_WAIT_FOREVER 失败。
 */
int clock_time_to_tick(const struct timespec *time, bool is_abs_timeout)
{
    int tick = 0;
    long nsecond, second;
    struct timespec tp;

    if (time == NULL)
    {
        return TTOS_WAIT_FOREVER;
    }

    memset(&tp, 0, sizeof(tp));

    if (time->tv_sec != 0 || time->tv_nsec != 0)
    {
        if (is_abs_timeout)
        {
            /* get current tp */
            clock_gettime(CLOCK_REALTIME, &tp);
        }

        if ((time->tv_nsec - tp.tv_nsec) < 0)
        {
            nsecond = (long)NSEC_PER_SEC - (tp.tv_nsec - time->tv_nsec);
            second = time->tv_sec - tp.tv_sec - 1;
        }
        else
        {
            nsecond = time->tv_nsec - tp.tv_nsec;
            second = time->tv_sec - tp.tv_sec;
        }

        /*
         * Warning: NANOSECOND_PER_SECOND is unsigned long, division method
         * instruction will be `divu`. so then result is overflow undefined
         * behavior.
         */
        unsigned long long sys_tick_ns = get_sys_tick_ns();
        /*
            由于当前时刻点通常不可能为一个tick的起始时刻点，
            所以，按照tick休眠,实际休眠时间会小于期望休眠时间,
            futex中要求休眠时间对齐到系统时间，确保不会提前醒来,
            所以需要+1
        */
        tick = (int)(second * TTOS_GetSysClkRate() +
                     +(ROUNDUP(nsecond, sys_tick_ns) / (long)sys_tick_ns)) +
               1;

        if (tick < 0)
            tick = 0;
    }

    return tick;
}

/**
 * @brief 将时间转为ms
 *
 * 该函数将时间转为ms
 *
 * @param[in] time timespec64格式表示的时间
 * @retval TTOS_WAIT_FOREVER  永久等待。
 * @retval 非TTOS_WAIT_FOREVER ms值。
 */
uint64_t timespec64_to_ms(const struct timespec64 *time)
{
    if (time == NULL)
    {
        return TTOS_WAIT_FOREVER;
    }

    return (time->tv_sec * MSEC_PER_SEC + time->tv_nsec / NSEC_PER_MSEC);
}