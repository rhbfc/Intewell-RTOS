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
 * @brief 系统调用实现：删除POSIX定时器。
 *
 * 该函数实现了一个系统调用，用于删除由timer_create创建的POSIX定时器。
 * 删除定时器会停止所有待处理的通知。
 *
 * @param[in] timerid 要删除的定时器ID
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 操作成功。
 * @retval -EINVAL timerid无效。
 * @retval -EFAULT 参数指向无效内存。
 *
 * @note 1. 删除定时器会立即停止所有待处理的通知。
 *       2. 删除后timerid将变为无效。
 *       3. 进程终止时会自动删除所有定时器。
 *       4. 不能删除其他进程的定时器。
 */
DEFINE_SYSCALL(timer_delete, (timer_t timer_id))
{
	int count = 0;
	int ret = TIMER_RETRY;
	struct posix_timer *timer = NULL;

	/* 先查找,找到再从红黑树中删除 */
	timer = posix_timer_detach((int)(long)timer_id);
	if (!timer)
	{
		/* timer_id已经无效 */
		return -EINVAL;
	}

	/* 阻止信号投递和timer重启 */
	timer->posix_timer_signal_seq++;

	while (ret == TIMER_RETRY)
	{
		ret = timer->clock_ops->remove_timer(timer);
		count++;
		if (count > 10)
		{
			printk("warning!!! timer_delete fail count:%d\n", count);
		}
	}

	posix_timer_putref(timer);

	if (ret < 0)
	{
		return ret;
	}

	return 0;
}