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
#include <time/posix_timer.h>
#include <time/ktime.h>
#include <uaccess.h>
#include <errno.h>
#include <time.h>
#include <bits/alltypes.h>
#include <ttosProcess.h>

/**
 * @brief 系统调用实现：获取时钟时间。
 *
 * 该函数实现了一个系统调用，用于获取指定时钟的时间。
 *
 * @param[in] clk 时钟 ID。可以是以下值之一：
 *              - CLOCK_REALTIME：系统实时时钟。
 *              - CLOCK_MONOTONIC：不受系统时间调整影响的时钟。
 *              - CLOCK_PROCESS_CPUTIME_ID：进程 CPU 时间时钟。
 *              - CLOCK_THREAD_CPUTIME_ID：线程 CPU 时间时钟。
 *              - CLOCK_MONOTONIC_RAW：不受系统时间调整和 NTP 同步影响的时钟。
 *              - CLOCK_REALTIME_COARSE：系统实时时钟，低精度。
 *              - CLOCK_MONOTONIC_COARSE：不受系统时间调整影响的时钟，低精度。
 *              - CLOCK_BOOTTIME：从系统启动开始计时，包括睡眠时间。
 * @param[out] ts 指向 timespec32 结构体的指针，用于返回时钟时间。
 * @return 成功时返回 0，失败时返回负值错误码。
 * @retval -ENODEV clk指向了不可使用的设备
 * @retval -EINVAL 无效的时钟 ID。
 * @retval -EFAULT ts 指向了不可访问的地址空间。
 * @retval -ENOTSUP 时钟不支持获取时间。
 * @retval -EPREM 调用进程没有足够的权限。
 */
DEFINE_SYSCALL(clock_gettime32, (clockid_t clk, struct timespec32 __user *ts))
{
    struct timespec64 kts64;
    struct timespec32 kts32;
    int ret;

    ret = kernel_clock_gettime(clk, &kts64);

    if(ret < 0)
    {
        return ret;
    }

    kts32.tv_sec = kts64.tv_sec;
    kts32.tv_nsec = kts64.tv_nsec;

    return copy_to_user(ts, &kts32, sizeof(kts32));

    return 0;
}
