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
#include <sched.h>
#include <ttos.h>
#include <errno.h>
#include <ttosInterTask.inl>
#include <ttosHal.h>
#include <uaccess.h>

/**
 * @brief 系统调用实现：设置进程的CPU亲和性。
 *
 * 该函数实现了一个系统调用，用于设置指定进程可以运行的CPU集合。
 *
 * @param[in] pid 进程ID（0表示当前进程）
 * @param[in] len CPU集合的大小（字节数）
 * @param[in] user_mask_ptr CPU亲和性掩码指针
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功。
 * @retval -EINVAL 参数无效。
 * @retval -ESRCH 进程不存在。
 * @retval -EFAULT 无效的用户空间地址。
 *
 * @note 1. 需要特权。
 *       2. 支持SMP。
 *       3. 进程级设置。
 *       4. 立即生效。
 */
DEFINE_SYSCALL (sched_setaffinity, (pid_t pid, unsigned int len,
                                     unsigned long __user *user_mask_ptr))
{
    T_TTOS_ReturnCode core_ret;
    int result = 0;
    cpu_set_t kcpu_set;
    int ret;

    CPU_ZERO (&kcpu_set);
    TASK_ID task; 

    if (pid == 0)
    {
        task = ttosGetRunningTask();
    }
    else
    {
        task = (TASK_ID)task_get_by_tid(pid);
    }

    if (task == NULL)
    {
        result = -ESRCH;
    }

    ret = copy_from_user(&kcpu_set, user_mask_ptr, sizeof(cpu_set_t));
    if (ret)
    {
        result = -EFAULT;
    }

    core_ret = TTOS_SetTaskAffinity(task, &kcpu_set);
    if (core_ret != TTOS_OK)
    {
        result = -EINVAL; 
    }

    return result;
}
