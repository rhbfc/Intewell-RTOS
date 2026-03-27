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

int vfs_mq_unlink (const char *mq_name);

/**
 * @brief 系统调用实现：删除消息队列。
 *
 * 该函数实现了一个系统调用，用于删除消息队列。
 *
 * @param name 消息队列名称。
 * @return 成功时返回 0，失败时返回负值错误码。
 */
DEFINE_SYSCALL (mq_unlink, (const char __user *name))
{
    int ret;
    if (!user_access_check (name, sizeof (long), UACCESS_R))
    {
        return -EINVAL;
    }
    const char *kname = strdup (name);

    ret = vfs_mq_unlink (kname);
    free ((void *)kname);
    return ret;
}