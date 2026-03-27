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

/**
 * @brief 将两个timespec结构表示的时间相减
 *
 * 该函数将两个timespec结构表示的时间相减
 *
 * @param[in] ts1 指向第一个timespec结构体的指针
 * @param[in] ts2 指向第二个timespec结构体的指针
 * @return ts1和ts2相减的差
 * @retval ts1和ts2相减的差
 */
struct timespec clock_timespec_subtract (const struct timespec *ts1,
                                         const struct timespec *ts2)
{
    time_t          sec;
    long            nsec;
    struct timespec ret;

    if (ts1->tv_sec < ts2->tv_sec)
    {
        sec  = 0;
        nsec = 0;
    }
    else if (ts1->tv_sec == ts2->tv_sec && ts1->tv_nsec <= ts2->tv_nsec)
    {
        sec  = 0;
        nsec = 0;
    }
    else
    {
        sec = ts1->tv_sec - ts2->tv_sec;

        if (ts1->tv_nsec < ts2->tv_nsec)
        {
            nsec = (ts1->tv_nsec + NSEC_PER_SEC) - ts2->tv_nsec;
            sec--;
        }
        else
        {
            nsec = ts1->tv_nsec - ts2->tv_nsec;
        }
    }

    ret.tv_sec  = sec;
    ret.tv_nsec = nsec;

    return ret;
}
