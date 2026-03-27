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
#include <errno.h>
#include <sched.h>
#include <ttos.h>
#include <ttosInterTask.inl>

/**
 * @brief 系统调用实现：获取进程的调度策略。
 *
 * 该函数实现了一个系统调用，用于获取指定进程的调度策略。
 *
 * @param[in] tid 进程ID（0表示当前进程）
 * @return 成功时返回调度策略，失败时返回负值错误码。
 * @retval SCHED_FIFO 先进先出调度。
 * @retval SCHED_RR 时间片轮转调度。
 * @retval SCHED_OTHER 其他调度策略。
 * @retval -EINVAL 参数无效。
 * @retval -ESRCH 进程不存在。
 * @retval -EPERM 权限不足。
 *
 * @note 1. 需要特权。
 *       2. 进程级查询。
 *       3. 立即生效。
 *       4. 策略独立。
 */
DEFINE_SYSCALL(sched_getscheduler, (pid_t tid))
{
	int policy;
	TASK_ID task = (TASK_ID)task_get_by_tid(tid);

	if (tid == 0)
	{
		task = ttosGetRunningTask();
	}

	if (task == NULL)
	{
		return -ESRCH;
	}

	policy = task->taskSchedPolicy;

	return policy;
}
