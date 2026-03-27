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
/* @MOD_HEAD> */

/************************宏 定 义******************************/
/************************类型定义******************************/
/************************外部声明******************************/
/************************前向声明******************************/
T_MODULE T_VOID ttosExtractFifo (T_TTOS_TaskControlBlock *theTask);
T_MODULE T_VOID ttosExtractPriority (T_TTOS_TaskControlBlock *theTask);
/************************模块变量******************************/

/* @MODULE> */
/*
 * @brief
 *       将指定的任务从其队列中移出。
 * @param[in]  queue: 任务所在的队列
 * @param[in]  theTask: 待移出的任务
 * @return
 *       none
 * @implements: RTE_DTASK.11.1
 */
void ttosExtractTaskq (T_TTOS_Task_Queue_Control *theTaskQueue,
                       T_TTOS_TaskControlBlock   *theTask)
{
    /* @REPLACE_BRACKET:
     * 任务的等待队列指针有可能被清空，表示已经被移出了，不需要再次移出 */
    if (theTaskQueue != NULL)
    {
        /* @REPLACE_BRACKET: 根据队列的类型采用不同的移出方式*/
        if (theTaskQueue->discipline == T_TTOS_QUEUE_DISCIPLINE_FIFO)
        {
            (void)ttosExtractFifo (theTask);
        }

        else
        {
            (void)ttosExtractPriority (theTask);
        }

        theTask->wait.queue = NULL;
    }
}

/*
 * @brief
 *       将指定的任务从先进先出队列中移出。
 * @param[in]  theTask: 待移出的任务
 * @return
 *       none
 * @implements: RTE_DTASK.11.2
 */
T_MODULE void ttosExtractFifo (T_TTOS_TaskControlBlock *theTask)
{
    /* @REPLACE_BRACKET:  theTask->resourceNode.objectNode.next != NULL*/
    if (theTask->resourceNode.objectNode.next != NULL)
    {
        /*
         * @KEEP_COMMENT: 调用list_delete(DT.8.8)将theTask任务从任务
         * 等待队列中移除
         */
        list_delete(&(theTask->resourceNode.objectNode));
    }
}

/*
 * @brief
 *       将指定的任务从优先级队列中移出。
 * @param[in]  theTask: 待移出的任务
 * @param[in]  removeWd: 为TRUE表示需移除定时器，为FALSE表示不移除定时器
 * @return
 *       none
 * @implements: RTE_DTASK.11.3
 */
T_MODULE void ttosExtractPriority (T_TTOS_TaskControlBlock *theTask)
{
    struct list_node *nextNode;
    T_TTOS_TaskControlBlock *newFirstTask;
    struct list_node *newFirstNode;
    struct list_node *samePriNode;

    /* @REPLACE_BRACKET:  theTask->resourceNode.objectNode.next != NULL */
    if (theTask->resourceNode.objectNode.next != NULL)
    {
        nextNode = theTask->resourceNode.objectNode.next;
        if (nextNode == NULL)
        {
            return;
        }

        /* @REPLACE_BRACKET: 检查是否有同优先级的等待任务*/
        if (list_is_empty(&theTask->wait.samePriQueue) == FALSE)
        {
            /* @KEEP_COMMENT: 移出指定的任务控制块节点*/
            newFirstNode = list_delete_head(&theTask->wait.samePriQueue);
            newFirstTask
                = (T_TTOS_TaskControlBlock
                       *)(((T_TTOS_ResourceTaskNode *)newFirstNode)->task);
            if (newFirstTask == NULL)
            {
                return;
            }
            list_replace(&theTask->resourceNode.objectNode, newFirstNode);
            theTask->resourceNode.objectNode.next = NULL;
            theTask->resourceNode.objectNode.prev = NULL;

            INIT_LIST_HEAD(&newFirstTask->wait.samePriQueue);
            while (list_is_empty(&theTask->wait.samePriQueue) == FALSE)
            {
                samePriNode = list_delete_head(&theTask->wait.samePriQueue);
                list_add_tail(samePriNode, &newFirstTask->wait.samePriQueue);
            }
        }

        else /*没有同优先级的则直接从链表中移除该节点*/
        {
            list_delete(&(theTask->resourceNode.objectNode));
        }
        INIT_LIST_HEAD(&theTask->wait.samePriQueue);
    }
}
