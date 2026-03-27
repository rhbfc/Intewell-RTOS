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
 * @brief 将两个timespec64结构表示的时间相加
 *
 * 该函数将两个timespec64结构表示的时间相加
 *
 * @param[in] time1 指向第一个timespec64结构体的指针
 * @param[in] time2 指向第二个timespec64结构体的指针
 * @return time1和time2累加的和
 * @retval time1和time2累加的和
 */
struct timespec64 clock_timespec_add64 (const struct timespec64 *time1,
                                        const struct timespec64 *time2)
{
    struct timespec64 ret;

    ret.tv_sec  = time1->tv_sec + time2->tv_sec;
    ret.tv_nsec = time1->tv_nsec + time2->tv_nsec;

    if (ret.tv_nsec >= NSEC_PER_SEC)
    {
        ret.tv_nsec -= NSEC_PER_SEC;
        ret.tv_sec++;
    }

    return ret;
}
