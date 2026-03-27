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
 * @brief 设置时钟时间。
 *
 * @param[in] tp 指向 timespec64 结构体的指针，用于设置时钟时间。
 * @param[in] tzp 指向 timezone 结构体的指针
 * @return 成功时返回 0，失败时返回负值错误码。
 * @retval -EINVAL  参数无效
 */
int kernel_settimeofday (const struct timeval *tp, const struct timezone *tzp)
{
    struct timespec64 ts;

    ts.tv_sec  = tp->tv_sec;
    ts.tv_nsec = tp->tv_usec * NSEC_PER_USEC;

    kernel_clock_settime (CLOCK_REALTIME, &ts);

    return 0;
}
