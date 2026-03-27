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
#include <bits/alltypes.h>
#include <ttosProcess.h>

/**
 * @brief 系统调用实现：获取定时器的时间。
 *
 * 该函数实现了一个系统调用，用于获取定时器的时间。
 *
 * @param[in] t 定时器 ID。
 * @param[out] val 指向用户空间的 itimerspec32 结构体指针，用于返回定时器的时间。
 * @return 成功时返回 0，失败时返回负值错误码。
 * @retval -EINVAL 定时器 ID 无效。
 * @retval -EFAULT 拷贝数据到用户空间失败。
 * @retval -ENOTSUP 不支持的操作。
 */
DEFINE_SYSCALL(timer_gettime64, (timer_t t, struct itimerspec64 __user *val))
{
    struct itimerspec64 cur_setting = {0};

	int ret = do_timer_gettime(t, &cur_setting);
	if (!ret) 
	{
		return copy_to_user(val, &cur_setting, sizeof(*val));
	}

	return ret;
}