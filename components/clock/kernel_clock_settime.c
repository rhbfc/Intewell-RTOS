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

#include <errno.h>
#include <time/ktime.h>
#include <time/posix_timer.h>

/**
 * @brief 设置时钟时间。
 *
 * @param[in] clock_id 时钟 ID。
 *              - CLOCK_REALTIME：系统实时时钟。
 *              - CLOCK_MONOTONIC：不受系统时间调整影响的时钟。
 *              - CLOCK_PROCESS_CPUTIME_ID：进程 CPU 时间时钟。
 *              - CLOCK_THREAD_CPUTIME_ID：线程 CPU 时间时钟。
 *              - CLOCK_MONOTONIC_RAW：不受系统时间调整和 NTP 同步影响的时钟。
 *              - CLOCK_REALTIME_COARSE：系统实时时钟，低精度。
 *              - CLOCK_MONOTONIC_COARSE：不受系统时间调整影响的时钟，低精度。
 *              - CLOCK_BOOTTIME：从系统启动开始计时，包括睡眠时间。
 * @param[in] tp 指向 timespec64 结构体的指针，用于设置时钟时间。
 * @return 成功时返回 0，失败时返回负值错误码。
 * @retval -EINVAL  参数无效
 * @retval -ENOTSUP 此功能不支持
 */
int kernel_clock_settime (clockid_t clock_id, const struct timespec64 *tp)
{
    const struct posix_clock_ops *clock_ops;

    clock_ops = clock_id_to_ops(clock_id);
    if (clock_ops == NULL)
        return -EINVAL;

    if (!clock_ops->update_time)
    {
        return -ENOTSUP;
    }

    return clock_ops->update_time(clock_id, tp);
}
