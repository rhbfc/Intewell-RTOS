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
#include <klog.h>

DEFINE_SYSCALL (timer_settime32,
                (timer_t t, int flags, struct itimerspec32 __user *val,
                 struct itimerspec32 __user *old))
{
    struct itimerspec32 k_val32 = {0};
	struct itimerspec   k_val_out32 = {0};
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

    if (copy_from_user (&k_val32, val, sizeof (k_val32)))
	{
        posix_timer_putref(timer);
        return -EFAULT;
	}

    k_val64.it_interval.tv_sec  = k_val32.it_interval.tv_sec;
    k_val64.it_interval.tv_nsec = k_val32.it_interval.tv_nsec;
    k_val64.it_value.tv_sec     = k_val32.it_value.tv_sec;
    k_val64.it_value.tv_nsec    = k_val32.it_value.tv_nsec;

	kits64 = old ? &old_spec : NULL;
	ret = do_timer_settime(t, flags, &k_val64, kits64);
	if (!ret && old) 
	{
		k_val_out32.it_interval.tv_sec  = kits64->it_interval.tv_sec;
		k_val_out32.it_interval.tv_nsec = kits64->it_interval.tv_nsec;
		k_val_out32.it_value.tv_sec     = kits64->it_value.tv_sec;
		k_val_out32.it_value.tv_nsec    = kits64->it_value.tv_nsec;

		if (copy_to_user (old, &k_val_out32, sizeof (*old)))
		{
            posix_timer_putref(timer);
            return -EFAULT;
		}
	}

    posix_timer_putref(timer);
	return ret;
}