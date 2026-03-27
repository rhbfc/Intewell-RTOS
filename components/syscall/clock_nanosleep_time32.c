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
#include <bits/alltypes.h>
#include <errno.h>
#include <time.h>
#include <time/ktime.h>
#include <time/posix_timer.h>
#include <ttosProcess.h>
#include <uaccess.h>
#include <time/ktimer.h>
#include <restart.h>

/**
 * @brief 系统调用实现：休眠。
 *
 * 该函数实现了一个系统调用，用于休眠。
 *
 * @param[in] clk 时钟 ID。可以是以下值之一：
 *              - CLOCK_REALTIME：系统实时时钟。
 *              - CLOCK_TAI：系统时钟，忽略闰秒。
 *              - CLOCK_MONOTONIC：不受系统时间调整影响的时钟。
 *              - CLOCK_PROCESS_CPUTIME_ID：进程 CPU 时间时钟。
 *              - CLOCK_BOOTTIME：从系统启动开始计时，包括睡眠时间。
 * @param[in] flags 休眠标志。可以是以下值之一：
 *             - TIMER_ABSTIME：rqtp 指定的时间是绝对时间。
 *             - 0：rqtp 指定的时间是相对时间。
 * @param[in] _rqtp 指向 timespec32 结构体的指针，用于指定休眠时间。
 * @param[out] _rmtp 指向 timespec32 结构体的指针，用于返回剩余休眠时间。
 * @return 成功时返回 0，失败时返回负值错误码。
 * @retval -EFAULT rqtp 或 rmtp 指向了不可访问的地址空间。
 * @retval -ENOTSUP 时钟不支持休眠。
 * @retval -EINTR 休眠被信号中断。
 * @retval -EINVAL 无效的时钟 ID或休眠时间参数错误。
 *
 */
DEFINE_SYSCALL (clock_nanosleep_time32,
                 (clockid_t clk, int flags, struct timespec32 __user *rqtp,
                  struct timespec32 __user *rmtp))
{
	struct timespec64 ts64 = {0};
	struct timespec32 ts32 = {0};
	ktime_t time = 0;
    pcb_t cur = ttosProcessSelf();

	switch (clk) 
    {
        case CLOCK_REALTIME:
        case CLOCK_MONOTONIC:
        case CLOCK_BOOTTIME:
            break;
        default:
            return -EINVAL;
	}

	if (copy_from_user(&ts32, rqtp, sizeof(ts32)))
	{
		return -EFAULT;
	}

	ts64.tv_sec  = ts32.tv_sec;
	ts64.tv_nsec = ts32.tv_nsec;

	if (!timespec64_valid(&ts64))
	{
		return -EINVAL;
	}

	if (flags & TIMER_ABSTIME)
	{
		rmtp = NULL;
	}

	restart_init_nanosleep(cur, restart_return_eintr, (void *)rmtp, TIMESPEC32);

	time = timespec64_to_ktime(ts64);

	return ktimer_nanosleep(time, flags & TIMER_ABSTIME ? KTIMER_MODE_ABS : KTIMER_MODE_REL,clk);
}
