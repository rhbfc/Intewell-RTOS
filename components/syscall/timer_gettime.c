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
#include <time/posix_timer.h>
#include <ttosProcess.h>

/**
 * @brief 系统调用实现：获取POSIX定时器的当前值。
 *
 * 该函数实现了一个系统调用，用于获取指定POSIX定时器的当前值和下次到期时间。
 *
 * @param[in] timerid 定时器ID
 * @param[out] curr_value 定时器当前值结构体：
 *                      - it_value: 距离下次到期的时间
 *                        - tv_sec: 秒数
 *                        - tv_nsec: 纳秒数
 *                      - it_interval: 定时器间隔
 *                        - tv_sec: 秒数
 *                        - tv_nsec: 纳秒数
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 操作成功。
 * @retval -EINVAL timerid无效。
 * @retval -EFAULT curr_value指向无效内存。
 *
 * @note 1. it_value为0表示定时器已过期或未启动。
 *       2. it_interval为0表示定时器是一次性的。
 *       3. 时间精度为纳秒级。
 *       4. 定时器运行时值会持续减小。
 */
DEFINE_SYSCALL(timer_gettime, (timer_t t, struct itimerspec __user *val))
{
	struct itimerspec kval = {0};
    struct itimerspec64 cur_setting = {0};

	int ret = do_timer_gettime(t, &cur_setting);
	if (!ret) 
	{
		kval.it_interval.tv_sec  = cur_setting.it_interval.tv_sec;
		kval.it_interval.tv_nsec = cur_setting.it_interval.tv_nsec;
		kval.it_value.tv_sec     = cur_setting.it_value.tv_sec;
		kval.it_value.tv_nsec    = cur_setting.it_value.tv_nsec;

		return copy_to_user(val, &kval, sizeof(*val));
	}

	return ret;
}