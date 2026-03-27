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
#include <linux/uaccess.h>

/**
 * @brief 系统调用实现：设置系统时间和时区。
 *
 * 该函数实现了一个系统调用，用于设置系统时间和时区信息。
 * 这是一个较老的接口，新代码应该使用clock_settime()。
 *
 * @param[in] tp 时间值结构体指针：
 *              - tv_sec: 自Unix纪元以来的秒数
 *              - tv_usec: 微秒部分（0-999999）
 * @param[in] tzp 时区结构体指针（已废弃，应始终为NULL）：
 *              - tz_minuteswest: 格林威治以西的分钟数
 *              - tz_dsttime: 夏令时调整类型
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 操作成功。
 * @retval -EFAULT tp或tzp指向无效内存。
 * @retval -EINVAL tp包含无效时间值。
 * @retval -EPERM 没有权限设置系统时间。
 *
 * @note 1. 设置系统时间需要特权权限。
 *       2. tzp参数已被废弃，不应再使用。设置时区应通过/etc/timezone
 *          或TZ环境变量来完成。
 *       3. 建议使用clock_settime(CLOCK_REALTIME, ...)代替此函数。
 */
DEFINE_SYSCALL(settimeofday, (const struct timeval __user *tp,
    const struct timezone __user *tzp))
{
    struct timespec64 kts64;
	struct timezone ktz;

	if (tp) 
    {
		if (get_user(kts64.tv_sec, &tp->tv_sec) ||
		    get_user(kts64.tv_nsec, &tp->tv_usec))
        {
            return -EFAULT;
        }

		if (kts64.tv_nsec > USEC_PER_SEC || kts64.tv_nsec < 0)
        {
			return -EINVAL;
        }

		kts64.tv_nsec *= NSEC_PER_USEC;
	}

	if (tzp) 
    {
		if (copy_from_user(&ktz, tzp, sizeof(*tzp)))
			return -EFAULT;
	}

	return kernel_settimeofday64(tp ? &kts64 : NULL, tzp ? &ktz : NULL);
}

__alias_syscall(settimeofday, settimeofday_time32);
