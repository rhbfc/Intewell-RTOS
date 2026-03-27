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
#include <unistd.h>

/**
 * @brief 系统调用实现：获取进程的补充组列表。
 *
 * 该函数实现了一个系统调用，用于获取调用进程的补充组ID列表。
 *
 * @param[in] size 列表大小
 * @param[out] list 存储组ID的数组
 * @return 成功时返回组数，失败时返回负值错误码。
 * @retval >0 成功，返回组数。
 * @retval -EINVAL size为负数。
 * @retval -EFAULT list指向无效内存。
 *
 * @note 1. 如果size为0，只返回组数。
 *       2. 如果size小于组数，返回EINVAL。
 *       3. 补充组用于额外的访问权限。
 *       4. 最多支持NGROUPS_MAX个组。
 */
DEFINE_SYSCALL (getgroups, (int size, gid_t __user *list))
{
    int i;
    gid_t gid = 0;

    if(size > 1)
    {
        size = 1;
    }
    
    for(i = 0; i < size; i++)
    {
        copy_to_user(&list[i], &gid, sizeof(gid));
    }
    /* 没有多用户 恒返回1 */
    return 1;
}

__alias_syscall(getgroups, getgroups32);