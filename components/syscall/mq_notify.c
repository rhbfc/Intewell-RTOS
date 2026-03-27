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
#include <assert.h>
#include <time/ktime.h>
#include <errno.h>
#include <fs/fs.h>
#include <stdlib.h>
#include <ttosProcess.h>
#include <uaccess.h>
#include <mqueue.h>
#include "mqueue.h"

int vfs_mq_notify (mqd_t mqdes, struct sigevent *notification);

/**
 * @brief 系统调用实现：消息队列通知。
 *
 * 该函数实现了一个系统调用，用于消息队列通知。
 *
 * @param mqdes 消息队列描述符。
 * @param notification 指向用户空间的信号事件结构体指针。
 * @return 成功时返回 0，失败时返回负值错误码。
 */
DEFINE_SYSCALL (mq_notify,
                     (mqd_t mqdes, const struct sigevent __user *notification))
{
    struct file *filep;
    int ret;
	struct sigevent *k_notification = NULL;

    ret = fs_getfilep(mqdes, &filep);
    if (ret < 0)
    {
      return ret;
    }

	if (notification && user_access_check (notification, sizeof (struct sigevent), UACCESS_R))
	{
		k_notification = malloc(sizeof(struct sigevent));

		if (k_notification)
		{
			copy_from_user(k_notification, notification, sizeof(struct sigevent));
		}
		else
		{
			return -ENOMEM;
		}
	}

	ret = vfs_mq_notify(mqdes, k_notification);

	if (k_notification)
	{
		free(k_notification);
		k_notification = NULL;
	}
	return ret;
}
