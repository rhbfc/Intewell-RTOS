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

#include <ttos.h>
#include <ttosInterHal.h>
#include <ttosUtils.inl>

T_VOID ttos_deinit_event(T_TTOS_EventControl *event)
{
    T_TTOS_TaskControlBlock *task;
    T_TTOS_TaskControlBlock *next;

    if (event == NULL)
    {
        return;
    }

    ttosDisableTaskDispatchWithLock();
    list_for_each_entry_safe(task, next, &event->wait_list, event_wait_node)
    {
        task->wait.option = 0U;
        task->wait.count = 0U;
        task->wait.returnCode = TTOS_UNSATISFIED;
        if (task->wait.returnArgument)
        {
            *(T_TTOS_EventSet *)task->wait.returnArgument = 0;
        }

        /* Keep delta-time waited queue consistent before waking task. */
        if (task->objCore.objectNode.next != NULL)
        {
            ttosExactWaitedTask(task);
        }

        /* @KEEP_COMMENT: 调用ttosClearTaskWaiting(DT.2.22)清除task任务的等待状态 */
        ttosClearTaskWaiting(task);

        /* 将任务从链表中删除 */
        list_del(&task->event_wait_node);
    }

    event->pendingEvents = 0;

    /* @KEEP_COMMENT: 调用ttosEnableTaskDispatchWithLock() 恢复调度 */
    (void)ttosEnableTaskDispatchWithLock();

    return;
}
