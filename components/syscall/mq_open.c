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

mqd_t vfs_mq_open (const char *mq_name, int oflags, ...);

#define MQ_MAX_MSG  10
#define MQ_MSG_SIZE 100

struct mq_attr default_attr = {0, MQ_MAX_MSG, MQ_MSG_SIZE, 0};

/**
 * @brief 系统调用实现：打开消息队列。
 *
 * 该函数实现了一个系统调用，用于打开消息队列。
 *
 * @param name 消息队列名称。
 * @param oflag 打开标志。
 * @param mode 权限。
 * @param attr 消息队列属性。
 * @return 成功时返回消息队列描述符，失败时返回负值错误码。
 */
DEFINE_SYSCALL (mq_open, (const char __user *name, int oflag, mode_t mode,
                          struct mq_attr __user *attr))
{
    struct mq_attr kattr;
    long           ret;
    if (!user_access_check (name, sizeof (long), UACCESS_R))
    {
        return -EINVAL;
    }
    const char *kname = strdup (name);

    if (oflag & O_CREAT)
    {
        if (attr == NULL)
        {
            attr = &default_attr;
        }
        else if (!user_access_check (attr, sizeof (*attr), UACCESS_R))
        {
            free ((void *)kname);
            return -EINVAL;
        }
        
        if (attr->mq_msgsize <= 0 || attr->mq_maxmsg <= 0)
        {
            free ((void *)kname);
            return -EINVAL;
        }
        kattr = *attr;
    }

    ret = vfs_mq_open (kname, oflag, mode, &kattr);
    free ((void *)kname);
    return ret;
}
