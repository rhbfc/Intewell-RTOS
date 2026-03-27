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
 * @brief 系统调用实现：设置POSIX定时器的值（64位版本）。
 *
 * 该函数实现了一个系统调用，用于设置POSIX定时器的初始值和间隔值，使用64位时间结构。
 *
 * @param[in] t 定时器ID
 * @param[in] flags 标志：
 *                 - 0：相对时间
 *                 - TIMER_ABSTIME：绝对时间
 * @param[in] val 新的定时器值：
 *                - it_value：初始超时值
 *                - it_interval：定期超时间隔
 * @param[out] old 如果不为NULL，存储定时器的前一个值
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功设置定时器。
 * @retval -EINVAL 无效的定时器ID或定时器不属于当前进程。
 * @retval -EFAULT 内存访问错误。
 *
 * @note 1. 支持64位时间值，适用于长时间运行的系统。
 *       2. 如果it_value为0，定时器停止。
 *       3. 如果it_interval为0，定时器只触发一次。
 *       4. 当flags为TIMER_ABSTIME时，it_value被解释为绝对时间。
 */
DEFINE_SYSCALL (timer_settime64,
                (timer_t t, int flags, struct itimerspec64 __user *val,
                 struct itimerspec64 __user *old))
{
   	struct itimerspec64 new_spec, old_spec, *rtn;
	int error = 0;

	if (!val)
	{
		return -EINVAL;
	}

	if (copy_itimerspec64_from_user(&new_spec, val))
	{
		return -EFAULT;
	}

	rtn = old ? &old_spec : NULL;
	error = do_timer_settime(t, flags, &new_spec, rtn);
	if (!error && old) 
	{
			if (copy_itimerspec64_to_user(&old_spec, old))
			error = -EFAULT;
	}
	return error;
}
