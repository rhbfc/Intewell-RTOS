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

#ifndef __LINUX_WAIT_H__
#define __LINUX_WAIT_H__

#include <errno.h>
#include <list.h>
#include <ttos.h>
#include <ttosBase.h>
#include <restart.h>

typedef T_TTOS_Task_Queue_Control wait_queue_head_t;

#define init_waitqueue_head(q)                                                                     \
    ttosInitializeTaskq((q), T_TTOS_QUEUE_DISCIPLINE_FIFO, TTOS_TASK_WAITING_FOR_EVENT)

#define wait_event_with_flag(wq, condition, isinterruptible)                                       \
    ({                                                                                             \
        int __ret__ = 0;                                                                           \
        long __irq_flag__;                                                                         \
        for (;;)                                                                                   \
        {                                                                                          \
            ttosDisableTaskDispatchWithLock();                                                     \
            ttos_int_lock(__irq_flag__);                                                            \
            if (condition)                                                                         \
            {                                                                                      \
                ttos_int_unlock(__irq_flag__);                                                      \
                ttosEnableTaskDispatchWithLock();                                                  \
                break;                                                                             \
            }                                                                                      \
            ttosGetRunningTask()->wait.returnCode = 0;                                             \
                                                                                                   \
            ttosEnqueueTaskq((wq), TTOS_WAIT_FOREVER, isinterruptible);                            \
            ttos_int_unlock(__irq_flag__);                                                          \
            ttosEnableTaskDispatchWithLock();                                                      \
            __ret__ = ttos_ret_to_errno(ttosGetRunningTask()->wait.returnCode);                    \
                                                                                                   \
            if (isinterruptible && (__ret__ == EINTR || pcb_signal_pending((pcb_t)ttosGetRunningTask()->ppcb)))                  \
            {                                                                                      \
                __ret__ = -ERR_RESTART_IF_SIGNAL;                                                            \
                break;                                                                             \
            }                                                                                      \
                                                                                                   \
        }                                                                                          \
                                                                                                   \
        __ret__;                                                                                   \
    })

#define wait_event_interruptible(wq, condition) wait_event_with_flag(wq, condition, TRUE)

#define wait_event(wq, condition) wait_event_with_flag(wq, condition, FALSE)

static inline void wake_up_interruptible(wait_queue_head_t *wq)
{
    struct list_node *node = NULL, *next_node = NULL;
    T_TTOS_TaskControlBlock *task;

    /* @KEEP_COMMENT: 禁止调度器 */
    ttosDisableTaskDispatchWithLock();

    node = list_first(&(wq->queues.fifoQueue));

    while (node)
    {
        next_node = list_next(node, &(wq->queues.fifoQueue));
        task = (T_TTOS_TaskControlBlock *)(((T_TTOS_ResourceTaskNode *)node)->task);
        list_delete(node);

        if ((task != NULL))
        {
            /* @REPLACE_BRACKET: node任务在任务tick等待队列上 */
            if (task->objCore.objectNode.next != NULL)
            {
                /* @KEEP_COMMENT:
                 * 调用ttosExactWaitedTask(DT.2.24)将node任务从任务tick等待队列中移除
                 */
                (void)ttosExactWaitedTask(task);
            }

            (void)ttosClearTaskWaiting(task);
            task->wait.queue = NULL;
            task->wait.returnCode = TTOS_OK;
        }
        node = next_node;
    }

    /* @KEEP_COMMENT: 重新使能调度 */
    ttosEnableTaskDispatchWithLock();
}

#define wake_up(wq) wake_up_interruptible(wq)

#endif
