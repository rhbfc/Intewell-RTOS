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
#include <errno.h>
#include <fs/fs.h>
#include <stdlib.h>
#include <ttosProcess.h>
#include <uaccess.h>
#include <mqueue.h>
#include <ttos.h>

extern int _mq_setattr(mqd_t mqdes, struct mq_attr *new, struct mq_attr *old);

/**
 * @brief 系统调用实现：获取或设置消息队列属性。
 *
 * 该函数实现了一个系统调用，用于获取或设置消息队列属性。
 *
 * @param mqdes 消息队列描述符。
 * @param mqstat 指向用户空间的消息队列属性结构体指针。
 * @param omqstat 指向用户空间的旧消息队列属性结构体指针。
 * @return 成功时返回 0，失败时返回负值错误码。
 */
DEFINE_SYSCALL (mq_getsetattr,
                     (mqd_t mqdes, const struct mq_attr __user *mqstat,
                      struct mq_attr __user *omqstat))
{
    size_t size = sizeof(struct mq_attr);
    struct mq_attr *new = NULL;
    struct mq_attr *old = NULL;
    int ret = 0;

    if (mqstat != NULL)
    {
        if (!user_access_check (mqstat, sizeof (*mqstat), UACCESS_R))
        {
            ret = -EINVAL;
            goto failed;
        }

        new = malloc(size);
        if (new == NULL)
        {
            ret = -ENOMEM;
            goto failed;
        }
        copy_from_user(new, mqstat, size);
    }

    if (omqstat != NULL)
    {
        if (!user_access_check (omqstat, sizeof (*omqstat), UACCESS_R))
        {
            ret = -EINVAL;
            goto failed;
        }

        old = malloc(size);
        if (old == NULL)
        {
            ret = -ENOMEM;
            goto failed;
        }
        copy_from_user(old, omqstat, size);
    }

    ret = _mq_setattr(mqdes, new, old);
    if (ret < 0)
    {
        ret = -EBADF;
        goto failed;
    }

    if (omqstat != NULL)
    {
        copy_to_user(omqstat, old, size);
    }

failed:
    if (new)
    {
        free(new);
        new = NULL;
    }

    if (old)
    {
        free(old);
        old = NULL;
    }
    return ret;
}
