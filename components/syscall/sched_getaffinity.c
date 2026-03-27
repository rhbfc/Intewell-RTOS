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
#include <driver/cpudev.h>

/**
 * @brief 系统调用实现：获取进程的CPU亲和性。
 *
 * 该函数实现了一个系统调用，用于获取指定进程可以运行的CPU集合。
 *
 * @param[in] pid 进程ID（0表示当前进程）
 * @param[in] len CPU集合的大小（字节数）
 * @param[out] user_mask_ptr CPU亲和性掩码指针
 * @return 成功时返回CPU集合大小，失败时返回负值错误码。
 * @retval CPU集合大小 成功。
 * @retval -EINVAL 参数无效。
 * @retval -ESRCH 进程不存在。
 * @retval -EFAULT 无法访问用户空间。
 *
 * @note 1. 需要特权。
 *       2. 支持SMP。
 *       3. 进程级设置。
 *       4. 立即生效。
 */
DEFINE_SYSCALL (sched_getaffinity, (pid_t pid, unsigned int len,
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
        return result;
    }

    core_ret = TTOS_GetTaskAffinity(task, &kcpu_set);
    if (core_ret != TTOS_OK)
    {
        result = -EINVAL; 
    }
    else
    {   
        /* 如果未设置亲核力 则设置每一个核的亲核力 */
        if(CPU_COUNT(&kcpu_set) == 0)
        {
            struct cpudev *dev;
            for_each_present_cpu(dev)
            {
                if(dev->state == CPU_STATE_RUNNING)
                    CPU_SET(dev->index, &kcpu_set);
            }
        }
        ret = copy_to_user(user_mask_ptr, &kcpu_set, sizeof(cpu_set_t));
        if (ret)
        {
            result = -EFAULT;
            return result;
        }
        result = len;
    }

    return result;
}
