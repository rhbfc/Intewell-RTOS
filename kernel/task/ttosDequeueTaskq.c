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

/* @<MODULE */

/************************头 文 件******************************/

/* @<MOD_HEAD */
#include <ttosBase.h>
#include <ttosHandle.h>
#include <ttosInterTask.inl>
/* @MOD_HEAD> */

/************************宏 定 义******************************/
/************************类型定义******************************/
/************************外部声明******************************/
T_EXTERN void ttosDisableTaskDispatchWithLock(void);
/************************前向声明******************************/
/************************实   现*******************************/

/* @MODULE> */
/*
 * @brief
 *       从指定的任务队列中移出一个任务。
 * @param[in]  theTaskQueue: 待操作的任务队列
 * @return:
 * 任务控制块指针
 * @implements: RTE_DTASK.6.1
 */
T_TTOS_TaskControlBlock *ttosDequeueTaskq(T_TTOS_Task_Queue_Control *theTaskQueue)
{
    T_TTOS_TaskControlBlock *task;
    TBSP_MSR_TYPE msr = 0U;
    TBSP_GLOBALINT_DISABLE(msr);

    /* @REPLACE_BRACKET: theTaskQueue->discipline ==
     * T_TTOS_QUEUE_DISCIPLINE_FIFO */
    if (theTaskQueue->discipline == T_TTOS_QUEUE_DISCIPLINE_FIFO)
    {
        /* @KEEP_COMMENT：获取fifo等待队列中第一个任务 */
        task = ttosDequeueFifo(theTaskQueue);
    }

    else
    {
        /* @KEEP_COMMENT：获取优先级等待队列中优先级最高的任务 */
        task = ttosDequeuePriority(theTaskQueue);
    }

    /* @REPLACE_BRACKET: task != NULL */
    if (task != NULL)
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
        TBSP_GLOBALINT_ENABLE(msr);
        /* @REPLACE_BRACKET: task */
        return (task);
    }

    TBSP_GLOBALINT_ENABLE(msr);
    /* @REPLACE_BRACKET: NULL */
    return (NULL);
}

/*
 * @brief
 *       从指定的任务队列中移出一个任务，但是不改变任务的状态。
 * @param[in]  theTaskQueue: 待操作的任务队列
 * @return:
 * 任务控制块指针
 * @implements: RTE_DTASK.6.2
 */
T_TTOS_TaskControlBlock *ttosDequeueTaskFromTaskq(T_TTOS_Task_Queue_Control *theTaskQueue)
{
    T_TTOS_TaskControlBlock *task;
    TBSP_MSR_TYPE msr = 0U;
    TBSP_GLOBALINT_DISABLE(msr);

    /* @REPLACE_BRACKET: theTaskQueue->discipline ==
     * T_TTOS_QUEUE_DISCIPLINE_FIFO */
    if (theTaskQueue->discipline == T_TTOS_QUEUE_DISCIPLINE_FIFO)
    {
        /* @KEEP_COMMENT：获取fifo等待队列中第一个任务 */
        task = ttosDequeueFifo(theTaskQueue);
    }

    else
    {
        /* @KEEP_COMMENT：获取优先级等待队列中优先级最高的任务 */
        task = ttosDequeuePriority(theTaskQueue);
    }

    /* @REPLACE_BRACKET: task != NULL */
    if (task != NULL)
    {
        /* @REPLACE_BRACKET: node任务在任务tick等待队列上 */
        if (task->objCore.objectNode.next != NULL)
        {
            /* @KEEP_COMMENT:
             * 调用ttosExactWaitedTask(DT.2.24)将node任务从任务tick等待队列中移除
             */
            (void)ttosExactWaitedTask(task);
        }

        task->wait.queue = NULL;
        task->wait.id = NULL;
        TBSP_GLOBALINT_ENABLE(msr);
        /* @REPLACE_BRACKET: task  */
        return (task);
    }

    TBSP_GLOBALINT_ENABLE(msr);
    /* @REPLACE_BRACKET: NULL  */
    return (NULL);
}

/*
 * @brief
 *       从先进先出的任务队列中移出一个任务。
 * @param[in]  theTaskQueue: 待操作的任务队列
 * @return
 *       移出的任务控制块
 * @implements: RTE_DTASK.6.3
 */
T_TTOS_TaskControlBlock *ttosDequeueFifo(T_TTOS_Task_Queue_Control *theTaskQueue)
{
    struct list_node *node = NULL;
    T_TTOS_TaskControlBlock *task;
    /* @KEEP_COMMENT: 获取fifo队列上的第一个节点 */
    node = list_first(&(theTaskQueue->queues.fifoQueue));

    /* @REPLACE_BRACKET: node != NULL  */
    if (node != NULL)
    {
        task = (T_TTOS_TaskControlBlock *)(((T_TTOS_ResourceTaskNode *)node)->task);
        /* @KEEP_COMMENT:
         * 调用list_delete(DT.8.8)将node任务从任务信号量等待队列上移除
         */
        list_delete(node);
        /* @REPLACE_BRACKET: task  */
        return (task);
    }

    /* @REPLACE_BRACKET: NULL  */
    return (NULL);
}

/*
 * @brief
 *       获取优先级等待队列中优先级最高的节点
 * @param[in]  theTaskQueue: 待操作的任务队列
 * @return
 *       移出的任务控制块
 * @implements: RTE_DTASK.6.4
 */
T_TTOS_TaskControlBlock *ttosDequeuePriority(T_TTOS_Task_Queue_Control *theTaskQueue)
{
    UINT32 idx;
    T_TTOS_TaskControlBlock *theTask = NULL;
    T_TTOS_TaskControlBlock *newFirstTask;
    struct list_node *node = NULL;
    struct list_node *newFirstNode;
    struct list_node *newSecondNode;
    struct list_node *lastNode;
    struct list_node *nextNode;
    struct list_node *previousNode;

    /* @KEEP_COMMENT：找到优先级等待队列上优先级最高的节点 */
    /* @REPLACE_BRACKET: idx = U(0);idx <
     * TTOS_QUEUE_DATA_NUMBER_OF_PRIORITY_HEADERS;idx++ */
    for (idx = 0U; idx < TTOS_QUEUE_DATA_NUMBER_OF_PRIORITY_HEADERS; idx++)
    {
        /* @REPLACE_BRACKET:
         * list_is_empty(&(theTaskQueue->queues.priorityQueue[idx])) == FALSE
         */
        if (list_is_empty((struct list_head *)&(theTaskQueue->queues.priorityQueue[idx])) ==
            FALSE)
        {
            node = list_first(
                (struct list_head *)&(theTaskQueue->queues.priorityQueue[idx]));
            break;
        }
    }

    /* @REPLACE_BRACKET: node != NULL  */
    if (node != NULL)
    {
        theTask = (T_TTOS_TaskControlBlock *)(((T_TTOS_ResourceTaskNode *)node)->task);
        if (NULL == theTask)
        {
            return (NULL);
        }
        /* @KEEP_COMMENT: 移出优先级最高的任务*/
        newFirstNode = list_first(&theTask->wait.samePriQueue);
        nextNode = theTask->resourceNode.objectNode.next;
        previousNode = theTask->resourceNode.objectNode.prev;
        if ((NULL == nextNode) || (NULL == previousNode))
        {
            return (NULL);
        }
        if (NULL != newFirstNode)
        {
            newFirstTask = (T_TTOS_TaskControlBlock *)(((T_TTOS_ResourceTaskNode *)newFirstNode)->task);
            if (NULL == newFirstTask)
            {
                return (NULL);
            }
        }

        /* @REPLACE_BRACKET: 检查是否有同优先级的等待任务*/
        if (list_is_empty(&theTask->wait.samePriQueue) == FALSE)
        {
            lastNode = list_tail(&theTask->wait.samePriQueue);
            newSecondNode = newFirstNode->next;
            previousNode->next = newFirstNode;
            nextNode->prev = newFirstNode;
            newFirstNode->next = nextNode;
            newFirstNode->prev = previousNode;
            if (NULL == lastNode)
            {
                return (NULL);
            }

            if (newFirstNode != lastNode)
            {
                /* 有不止一个同优先级的，还需修改相同优先级等待链表的头尾节点 */
                newSecondNode->prev = &newFirstTask->wait.samePriQueue;
                newFirstTask->wait.samePriQueue.next = newSecondNode;
                newFirstTask->wait.samePriQueue.prev = lastNode;
                lastNode->next = &newFirstTask->wait.samePriQueue;
            }
        }

        else
        {
            previousNode->next = nextNode;
            nextNode->prev = previousNode;
        }
        theTask->resourceNode.objectNode.next = NULL;
        theTask->resourceNode.objectNode.prev = NULL;
    }

    /* @REPLACE_BRACKET: theTask */
    return (theTask);
}
