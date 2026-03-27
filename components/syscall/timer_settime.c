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
 * @brief 系统调用实现：设置POSIX定时器的值。
 *
 * 该函数实现了一个系统调用，用于设置指定POSIX定时器的值，
 * 包括初始到期时间和重复间隔。
 *
 * @param[in] timerid 定时器ID
 * @param[in] flags 控制标志：
 *                - 0: 相对时间
 *                - TIMER_ABSTIME: 绝对时间
 * @param[in] new_value 新的定时器值：
 *                    - it_value: 初始到期时间
 *                    - it_interval: 重复间隔
 * @param[out] old_value 原定时器值：
 *                     - NULL: 不获取原值
 *                     - 非NULL: 返回原定时器值
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 操作成功。
 * @retval -EINVAL timerid无效或new_value包含无效值。
 * @retval -EFAULT new_value或old_value指向无效内存。
 *
 * @note 1. 如果it_value为0，定时器将被停止。
 *       2. 如果it_interval为0，定时器将只触发一次。
 *       3. 当flags为TIMER_ABSTIME时，it_value被解释为绝对时间。
 */
DEFINE_SYSCALL (timer_settime,
                (timer_t t, int flags, struct itimerspec __user *val,
                 struct itimerspec __user *old))
{
	struct itimerspec k_val = {0};
	struct itimerspec k_val_out = {0};
	struct itimerspec64 k_val64 = {0};
	struct itimerspec64 old_spec, *kits64;
	int ret = 0;

	if (!val)
	{
		return -EINVAL;
	}

	struct posix_timer *timer = posix_timer_get((int)(long)t);

    if (!timer)
	{
		return -EINVAL;
	}

    if (timer->pcb->group_leader != ttosProcessSelf ()->group_leader)
	{
        posix_timer_putref(timer);
		return -EINVAL;
	}

    if (copy_from_user (&k_val, val, sizeof (k_val)))
	{
        posix_timer_putref(timer);
        return -EFAULT;
	}

    k_val64.it_interval.tv_sec  = k_val.it_interval.tv_sec;
    k_val64.it_interval.tv_nsec = k_val.it_interval.tv_nsec;
    k_val64.it_value.tv_sec     = k_val.it_value.tv_sec;
    k_val64.it_value.tv_nsec    = k_val.it_value.tv_nsec;

	kits64 = old ? &old_spec : NULL;
	ret = do_timer_settime(t, flags, &k_val64, kits64);
	if (!ret && old) 
	{
		k_val_out.it_interval.tv_sec  = kits64->it_interval.tv_sec;
		k_val_out.it_interval.tv_nsec = kits64->it_interval.tv_nsec;
		k_val_out.it_value.tv_sec     = kits64->it_value.tv_sec;
		k_val_out.it_value.tv_nsec    = kits64->it_value.tv_nsec;

		if (copy_to_user (old, &k_val_out, sizeof (*old)))
		{
            posix_timer_putref(timer);
            return -EFAULT;
		}
	}

    posix_timer_putref(timer);
	return ret;
}
