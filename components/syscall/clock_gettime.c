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
#include <fs/fs.h>
#include <errno.h>
#include <uaccess.h>
#include <time.h>
#include <time/ktime.h>

/**
 * @brief 系统调用实现：获取指定时钟源的当前时间。
 *
 * 该函数实现了一个系统调用，用于获取指定时钟源的当前时间值。
 * 不同的时钟源有不同的特性和用途，可以根据需要选择合适的时钟源。
 *
 * @param[in] clockid 时钟源标识符：
 *                  - CLOCK_REALTIME: 系统实时时钟，可被系统管理员修改
 *                  - CLOCK_MONOTONIC: 单调递增时钟，不受系统时间修改影响
 *                  - CLOCK_PROCESS_CPUTIME_ID: 调用进程的CPU时间
 *                  - CLOCK_THREAD_CPUTIME_ID: 调用线程的CPU时间
 *                  - CLOCK_BOOTTIME: 系统启动时间，包含系统休眠时间
 *                  - CLOCK_REALTIME_COARSE: 低精度实时时钟，但开销小
 *                  - CLOCK_MONOTONIC_COARSE: 低精度单调时钟，但开销小
 * @param[out] tp 时间结构体指针：
 *               - tv_sec: 秒数
 *               - tv_nsec: 纳秒数（0-999999999）
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 操作成功。
 * @retval -EFAULT tp指向无效内存。
 * @retval -EINVAL clockid无效。
 * @retval -EPERM 没有权限访问指定的时钟源。
 *
 * @note CLOCK_MONOTONIC是用于测量时间间隔的推荐时钟源，因为它不会
 *       受到系统时间调整的影响。而CLOCK_REALTIME则适合获取实际时间。
 */
DEFINE_SYSCALL(clock_gettime, (clockid_t clk, struct timespec *ts))
{
	int ret;
    struct timespec kts = {0};
	struct timespec64 kts64 = {0};

	ret = kernel_clock_gettime(clk, &kts64);
	if (ret)
    {
		return ret;
    }

    kts.tv_nsec = kts64.tv_nsec;
    kts.tv_sec  = kts64.tv_sec;

    if (copy_to_user(ts, &kts, sizeof(*ts)))
    {
        return -EFAULT;
    }

	return 0;
}
