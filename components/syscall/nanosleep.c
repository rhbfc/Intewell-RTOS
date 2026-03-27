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
#include <time.h>
#include <time/ktime.h>
#include <time/posix_timer.h>
#include <ttosProcess.h>
#include <uaccess.h>
#include <restart.h>
#include <time/ktimer.h>

/**
 * @brief 系统调用实现：纳秒级休眠。
 *
 * 该函数实现了一个系统调用，用于使当前进程休眠指定的时间。
 *
 * @param[in] _rqtp 请求的休眠时间：
 *               - tv_sec: 秒数
 *               - tv_nsec: 纳秒数（0-999999999）
 * @param[out] _rmtp 剩余的休眠时间（如果被中断）：
 *               - NULL: 不关心剩余时间
 *               - 非NULL: 返回剩余需要睡眠的时间
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功完成休眠。
 * @retval -EINVAL 参数无效。
 * @retval -EFAULT 内存访问错误。
 * @retval -EINTR 被信号中断。
 *
 * @note 1. 实际休眠时间可能大于请求时间。
 *       2. 可能被信号中断。
 *       3. _rmtp返回剩余时间。
 *       4. 支持纳秒级精度。
 */
DEFINE_SYSCALL (nanosleep, (long __user *_rqtp, long __user *_rmtp))
{
    struct timespec64 ts = {0};
    struct timespec64 tm = {0};
    long              ts32[2]     = {0,0};
    long              ts32_tmp[2] = {0,0};
    int               ret;

    pcb_t pcb = ttosProcessSelf();

    if (copy_from_user (ts32, _rqtp, sizeof (ts32)))
    {
        return -EFAULT;
    }

    ts.tv_sec  = ts32[0];
    ts.tv_nsec = ts32[1];

	if (!timespec64_valid(&ts))
    {
		return -EINVAL;
    }

	restart_init_nanosleep(pcb, restart_return_eintr, (void *)_rmtp, TIMESPEC);
	
    return ktimer_nanosleep(timespec64_to_ns(&ts), KTIMER_MODE_REL,
				 CLOCK_MONOTONIC);
}
