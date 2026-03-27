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

#include "futex.h"
#include "syscall_internal.h"
#include <assert.h>
#include <ttosBase.h>
#include <ttosProcess.h>
#include <uaccess.h>

/**
 * @brief 系统调用实现：设置 robust futex list。
 *
 * 该函数实现了一个系统调用，用于设置 robust futex list。
 *
 * @param[in] head robust futex list 头指针
 * @param[in] len robust futex list 长度
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功。
 * @retval -EINVAL len 小于 sizeof(struct robust_list_head)。
 * @retval -EFAULT head 指向无效内存。
 *
 * @note 1. 用于 robust futex 机制。
 *       2. 支持多线程。
 *       3. 用户空间地址。
 */

DEFINE_SYSCALL(set_robust_list, (void __user *head, size_t len))
{
    if (len < sizeof(struct robust_list_head))
    {
        return -EINVAL;
    }

    if (head && !user_access_check(head, sizeof(struct robust_list_head), UACCESS_R))
    {
        return -EFAULT;
    }

    ttosGetRunningTask()->robust_list_head = head;
    ttosGetRunningTask()->robust_list_len = len;
    return 0;
}
