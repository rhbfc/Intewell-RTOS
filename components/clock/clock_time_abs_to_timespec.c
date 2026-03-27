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
 * @brief 将指定时钟上的绝对时间转换为相对时间
 *
 * 该函数将指定时钟上的绝对时间转换为相对时间
 *
 * @param[in] id 时钟id
 * @param[out] result 转换后的相对时间
 * @param[in] abstime 绝对时间
 * @return 成功时返回 0，失败时返回-1
 * @retval 0  成功。
 * @retval -1 失败。
 */
int clock_time_abs_to_timespec (int id, struct timespec *result,
                                const struct timespec *abstime)
{
    struct timespec now;

    clock_gettime (id, &now);
    if ((abstime->tv_sec * NSEC_PER_SEC + abstime->tv_nsec)
        < (now.tv_sec * NSEC_PER_SEC + now.tv_nsec))
    {
        return -1;
    }

    *result = clock_timespec_subtract (abstime, &now);

    return 0;
}
