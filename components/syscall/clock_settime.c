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

/**
 * @brief 系统调用实现：设置指定时钟源的时间。
 *
 * 该函数实现了一个系统调用，用于设置指定时钟源的时间值。
 * 注意并非所有时钟源都支持设置时间，某些时钟源是只读的。
 *
 * @param[in] clockid 时钟源标识符：
 *                  - CLOCK_REALTIME: 系统实时时钟，可被设置
 *                  - CLOCK_MONOTONIC: 单调递增时钟，只读
 *                  - CLOCK_PROCESS_CPUTIME_ID: 进程CPU时间，只读
 *                  - CLOCK_THREAD_CPUTIME_ID: 线程CPU时间，只读
 *                  - CLOCK_BOOTTIME: 系统启动时间，只读
 * @param[in] tp 时间结构体指针：
 *              - tv_sec: 秒数
 *              - tv_nsec: 纳秒数（0-999999999）
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 操作成功。
 * @retval -EFAULT tp指向无效内存。
 * @retval -EINVAL clockid无效或tp包含无效时间值。
 * @retval -EPERM 没有权限设置时钟。
 * @retval -ENOTSUP 指定的时钟源不支持设置时间。
 *
 * @note 设置系统时间需要特权权限。修改系统时间可能会影响依赖时间的
 *       应用程序，应谨慎使用。建议使用NTP等自动时间同步机制。
 */
DEFINE_SYSCALL (clock_settime,
                (clockid_t clk, const struct timespec __user *ts))
{
    struct timespec   kts = {0};
	struct timespec64 kts64 = {0};
    ktime_t now = 0;

	if (CLOCK_REALTIME != clk)
    {
		return -EINVAL;
    }

    int ret = copy_from_user (&kts, ts, sizeof (kts));
    if (ret)
    {
        return -EFAULT;
    }

    kts64.tv_sec  = kts.tv_sec;
    kts64.tv_nsec = kts.tv_nsec;

    if (!timespec64_valid(&kts64))
    {
        return -EINVAL;
    }

    if ((unsigned long long)ts->tv_sec >= KTIME_SETTIME_SEC_MAX)
    {
        return -EINVAL;
    }

    ktime_t time = timespec64_to_ktime(kts64);
	now = ktime_get_real();

    ktime_set_real(time - now);

    return 0;
}
