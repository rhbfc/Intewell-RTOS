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

#include "completion.h"
#include <ttosUtils.inl>
/**
 * @brief
 *       接收完成量
 * @param[in]   comp:   完成量控制块
 * @param[in]   ticks: 接收的事件条件不满足时指定任务的等待时间，单位为tick。
 *              0:     表示任务不等待事件；
 *              TTOS_EVENT_WAIT_FOREVER:表示任务永久等待事件
 * @return
 *       TTOS_INVALID_ID: 完成量控制块为空
 *       TTOS_CALLED_FROM_ISR: 从中断中调用
 *       TTOS_INVALID_USER：当前任务为IDLE任务
 *       TTOS_RESOURCE_IN_USE: 已经有任务在等待完成量
 *       TTOS_TIMEOUT:         等待超时
 *       TTOS_OK:              接收成功
 * @implements:  RTE_DEVENT.2.1
 */
T_TTOS_ReturnCode ttos_WaitCompletion(T_TTOS_CompletionControl *comp, T_UWORD ticks, T_BOOL is_interruptible)
{
    T_TTOS_TaskControlBlock *task;
    size_t flag;

    if (comp == NULL)
    {
        return TTOS_INVALID_ID;
    }

    task = ttosGetRunningTask();

    if (TRUE == ttosIsISR ())
    {
        /* @REPLACE_BRACKET: TTOS_CALLED_FROM_ISR */
        return (TTOS_CALLED_FROM_ISR);
    }

    if (TRUE == ttosIsIdleTask (task))
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_USER */
        return (TTOS_INVALID_USER);
    }

    ttosDisableTaskDispatchWithLock();

    /* 没有调用释放接口 */
    if (comp->flag == TTOS_COMPLETION_WAIT)
    {
        /* 判断waitqueue中是否有任务 */
        if (!list_is_empty(&(comp->waitQueue.queues.fifoQueue)))
        {
            /* 已经有任务等待完成量 */
            ttosEnableTaskDispatchWithLock();
            return TTOS_RESOURCE_IN_USE;
        }

        if (ticks == 0)
        {
            ttosEnableTaskDispatchWithLock();
            return TTOS_TIMEOUT;
        }
        else
        {
            ttosEnqueueTaskq(&comp->waitQueue, ticks, is_interruptible);
        }
    }

    comp->flag = TTOS_COMPLETION_WAIT;
    ttosEnableTaskDispatchWithLock();

    return TTOS_OK;
}
