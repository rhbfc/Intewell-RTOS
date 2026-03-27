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

T_UBYTE corePriorityToPthread (int priority);

/**
 * @brief 系统调用实现：获取进程的调度参数。
 *
 * 该函数实现了一个系统调用，用于获取指定进程的调度参数。
 *
 * @param[in] pid 进程ID（0表示当前进程）
 * @param[out] param 调度参数结构
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功。
 * @retval -EINVAL 参数无效。
 * @retval -ESRCH 进程不存在。
 * @retval -EPERM 权限不足。
 *
 * @note 1. 需要特权。
 *       2. 策略相关。
 *       3. 进程级设置。
 *       4. 立即生效。
 */
DEFINE_SYSCALL (sched_getparam, (pid_t tid, struct sched_param __user *param))
{
    TASK_ID task = (TASK_ID)task_get_by_tid(tid);
    if(task != NULL)
    {
        param->sched_priority = corePriorityToPthread(task->taskPriority);
    }
    return 0;
}
