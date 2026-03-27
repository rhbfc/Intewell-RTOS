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
 * @brief 系统调用实现：获取当前系统时间。
 *
 * 该函数实现了一个系统调用，用于获取当前系统时间和时区信息。
 * 时间值是从1970年1月1日00:00:00 UTC开始的微秒计数。
 *
 * @param[out] tv 时间值结构体指针:
 *               - tv_sec: 自Unix纪元以来的秒数
 *               - tv_usec: 微秒部分（0-999999）
 * @param[out] tz 时区结构体指针（已废弃，应始终为NULL）:
 *               - tz_minuteswest: 格林威治以西的分钟数
 *               - tz_dsttime: 夏令时调整类型
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 操作成功。
 * @retval -EFAULT tv指向无效内存。
 *
 * @note tz参数已被废弃，不应再使用。获取时区信息应使用localtime()或
 *       类似函数。此参数的存在仅为保持与旧程序的兼容性。
 */
DEFINE_SYSCALL(gettimeofday, (struct timeval __user *tp,
    struct timezone __user *tzp))
{
    struct timespec64 ts = {0};
    struct timezone ktz  = {0};
    uint64_t us = 0;

	if (tp != NULL) 
    {
		read_realtime_ts64(&ts);

        if (copy_to_user(&tp->tv_sec, &ts.tv_sec, sizeof(tp->tv_sec)))
        {
            return -EFAULT;
        }

        us = ts.tv_nsec/1000;

        if (copy_to_user(&tp->tv_usec, &us, sizeof(tp->tv_usec)))
        {
            return -EFAULT;
        }
	}
    
	if (tzp != NULL)
    {
        g_tz_get(&ktz);

		if (copy_to_user(tzp, &ktz, sizeof(*tzp)))
        {
			return -EFAULT;
        }
	}

	return 0;
}

__alias_syscall(gettimeofday, gettimeofday_time32);
