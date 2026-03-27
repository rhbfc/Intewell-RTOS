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
 * @brief 系统调用实现：使调用进程基于指定时钟源睡眠。
 *
 * 该函数实现了一个系统调用，使当前进程暂停执行指定的时间。
 * 与nanosleep()不同，clock_nanosleep()允许指定时钟源，并可以选择
 * 相对时间或绝对时间。
 *
 * @param[in] clockid 时钟源标识符：
 *                  - CLOCK_REALTIME: 系统实时时钟
 *                  - CLOCK_TAI：系统时钟，忽略闰秒。
 *                  - CLOCK_MONOTONIC：不受系统时间调整影响的时钟。
 *                  - CLOCK_PROCESS_CPUTIME_ID：进程 CPU 时间时钟。
 *                  - CLOCK_BOOTTIME：从系统启动开始计时，包括睡眠时间。
 * @param[in] flags 控制标志：
 *                - 0: 相对时间
 *                - TIMER_ABSTIME: 绝对时间
 * @param[in] req 请求的睡眠时间：
 *               - tv_sec: 秒数
 *               - tv_nsec: 纳秒数（0-999999999）
 * @param[out] rem 如果睡眠被中断，存储剩余的睡眠时间（仅用于相对时间模式）：
 *               - NULL: 不关心剩余时间
 *               - 非NULL: 返回剩余需要睡眠的时间
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功睡眠指定时间。
 * @retval -EINVAL clockid无效或req指向无效的时间规范。
 * @retval -EFAULT req或rem指向无效内存。
 * @retval -EINTR 被信号中断（仅在相对时间模式下）。
 * @retval -ENOTSUP 不支持指定的时钟源。
 *
 * @note 当flags为TIMER_ABSTIME时，函数会睡眠到指定的绝对时间点，
 *       此时不会因信号中断而提前返回，也不使用rem参数。
 */
DEFINE_SYSCALL (clock_nanosleep,
                 (clockid_t clk, int flags, struct timespec __user *rqtp,
                  struct timespec __user *rmtp))
{
	struct timespec64 ts64 = {0};
	struct timespec ts = {0};
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

	if (copy_from_user(&ts, rqtp, sizeof(ts)))
	{
		return -EFAULT;
	}

	ts64.tv_sec = ts.tv_sec;
	ts64.tv_nsec = ts.tv_nsec;

	if (!timespec64_valid(&ts64))
	{
		return -EINVAL;
	}

	if (flags & TIMER_ABSTIME)
	{
		rmtp = NULL;
	}

	restart_init_nanosleep(cur, restart_return_eintr, (void *)rmtp, TIMESPEC);

	time = timespec64_to_ktime(ts64);

	return ktimer_nanosleep(time, flags & TIMER_ABSTIME ? KTIMER_MODE_ABS : KTIMER_MODE_REL,clk);
}
