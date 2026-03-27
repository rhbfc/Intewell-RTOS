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
 * @brief 系统调用实现：获取 robust futex list。
 *
 * 该函数实现了一个系统调用，用于获取 robust futex list。
 *
 * @param[in] pid 目标线程的PID，如果为0则获取当前线程的 robust futex list
 * @param[out] head_ptr 存储 robust futex list 头指针的用户空间地址
 * @param[out] len_ptr 存储 robust futex list 长度的用户空间地址
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功。
 * @retval -ESRCH 目标线程不存在。
 * @retval -EPERM 目标线程不属于当前进程。
 * @retval -EFAULT head_ptr 或 len_ptr 指向无效内存。
 *
 * @note 1. 用于 robust futex 机制。
 *       2. 支持多线程。
 *       3. 用户空间地址。
 */
DEFINE_SYSCALL(get_robust_list, (int pid, void __user *__user *head_ptr, size_t __user *len_ptr))
{
    TASK_ID task;
    void __user *head;
    unsigned long len;

    if (!user_access_check(head_ptr, sizeof(void *), UACCESS_W) ||
        !user_access_check(len_ptr, sizeof(size_t), UACCESS_W))
    {
        return -EFAULT;
    }

    if (pid == 0 || pid == ttosGetRunningTask()->tid)
    {
        task = ttosGetRunningTask();
    }
    else
    {
        task = task_get_by_tid(pid);
        if (!task)
        {
            return -ESRCH;
        }
        if (task->ppcb != ttosGetRunningTask()->ppcb)
        {
            return -EPERM;
        }
    }

    head = task->robust_list_head;
    len = task->robust_list_len;

    if (copy_to_user(head_ptr, &head, sizeof(head)) < 0)
    {
        return -EFAULT;
    }
    if (copy_to_user(len_ptr, &len, sizeof(len)) < 0)
    {
        return -EFAULT;
    }
    return 0;
}
