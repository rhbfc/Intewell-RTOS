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
/************************前向声明******************************/
/************************实   现*******************************/

/* @MODULE> */

/*
 * @brief
 *       将当前任务插入到指定的任务队列中。
 * @param[in]  theTaskQueue: 待插入的任务队列
 * @param[in]  ticks: 等待时间
 * @return
 * @implements: RTE_DTASK.8.8
 */
void ttosEnqueueTaskqEx (T_TTOS_TaskControlBlock *task, T_TTOS_Task_Queue_Control *theTaskQueue, T_UWORD ticks, T_BOOL is_interruptible)
{
    /*
     * @KEEP_COMMENT: 调用ttosClearTaskReady(DT.2.21)清除当前运行任务
     * 的就绪状态
     */
    (void)ttosClearTaskReady (task);
    /* @KEEP_COMMENT: 设置任务的状态 */
    task->state      = theTaskQueue->state;
    if (is_interruptible)
    {
        task->state |= TTOS_TASK_INTERRUPTIBLE_BY_SIGNAL;
    }
    else
    {
        task->state &= ~TTOS_TASK_INTERRUPTIBLE_BY_SIGNAL;
    }
    task->wait.queue = theTaskQueue;

    /* @REPLACE_BRACKET: <ticks>不等于TTOS_WAIT_FOREVER */
    if (ticks != TTOS_WAIT_FOREVER)
    {
        /* @KEEP_COMMENT: 设置当前运行任务的等待时间为<ticks> */
        task->waitedTicks = ticks;
        /*
         * @KEEP_COMMENT: 调用ttosInsertWaitedTask(DT.2.26)将当前运行任务
         * 插入任务tick等待队列
         */
        (void)ttosInsertWaitedTask (task);
    }

    /* @REPLACE_BRACKET: theTaskQueue->discipline ==
     * T_TTOS_QUEUE_DISCIPLINE_FIFO */
    if (theTaskQueue->discipline == T_TTOS_QUEUE_DISCIPLINE_FIFO)
    {
        /* @KEEP_COMMENT: 将任务资源节点加入到fifo等待队列中 */
        (void)ttosEnqueueFifo (&(theTaskQueue->queues.fifoQueue),
                               &(task->resourceNode.objectNode));
    }

    else
    {
        /* @KEEP_COMMENT: 将指定的任务插入信号量优先级队列 */
        (void)ttosEnqueuePriority (theTaskQueue, task);
    }
    return;
}

/*
 * @brief
 *       将当前任务插入到指定的任务队列中。
 * @param[in]  theTaskQueue: 待插入的任务队列
 * @param[in]  ticks: 等待时间
 * @return
 * @implements: RTE_DTASK.8.8
 */
void ttosEnqueueTaskq (T_TTOS_Task_Queue_Control *theTaskQueue, T_UWORD ticks, T_BOOL is_interruptible)
{
    T_TTOS_TaskControlBlock *task = ttosGetRunningTask ();

	ttosEnqueueTaskqEx (task, theTaskQueue, ticks, is_interruptible);
}

/*
 * @brief
 *       将指定的任务以先进先出的方式插入指定的队列。
 * @param[in]  chain: 待插入的任务队列
 * @param[in]  node: 待插入的任务资源节点
 * @return
 * @implements: RTE_DTASK.8.9
 */
void ttosEnqueueFifo (struct list_head *chain, struct list_node *node)
{
    /* @KEEP_COMMENT: 将等待的任务按照先进先出的原则插入到信号量队列的末尾 */
    list_add_tail(node, chain);
}

/*
 * @brief
 *       将指定的任务插入信号量优先级队列。
 * @param[in]  semaCB: 信号量控制块
 * @param[in]  theTask: 待插入的任务
 * @return
 * @implements: RTE_DTASK.8.10
 */
void ttosEnqueuePriority (T_TTOS_Task_Queue_Control *theTaskQueue,
                          T_TTOS_TaskControlBlock   *theTask)
{
    T_UWORD                  searchPriority = 0U;
    T_TTOS_TaskControlBlock *searchTask = NULL;
    struct list_head        *header;
    struct list_node        *recordNode;
    T_UWORD                  headerIndex;
    T_UWORD                  priority;
    T_UWORD                  tmpPriority;

    INIT_LIST_HEAD(&theTask->wait.samePriQueue);
    priority    = theTask->taskCurPriority;
    theTask->wait.queue = theTaskQueue;

    tmpPriority = priority % TTOS_QUEUE_DATA_PRIORITIES_PER_HEADER;
    /* @KEEP_COMMENT: 确定任务应插入到哪条优先级链表*/
    headerIndex = priority / TTOS_QUEUE_DATA_PRIORITIES_PER_HEADER;
    header      = &(theTaskQueue->queues.priorityQueue[headerIndex]);

    /* @REPLACE_BRACKET:   判断是从链表头还是尾开始搜索*/
    if (tmpPriority >= TTOS_QUEUE_DATA_REVERSE_SEARCH_LEVEL)
    {
        /* @KEEP_COMMENT: 从链表尾开始定位插入点*/
        recordNode = list_tail(header);

        /* @KEEP_COMMENT: 遍历优先级链表中每个节点的优先级以定位插入点*/
        while (recordNode != NULL)
        {
            /* @KEEP_COMMENT: 获取当前节点对应的任务块 */
            searchTask = (T_TTOS_TaskControlBlock
                              *)(((T_TTOS_ResourceTaskNode *)(recordNode))->task);
            /* @KEEP_COMMENT: 节点优先级 */
            searchPriority = searchTask->taskCurPriority;

            /* @REPLACE_BRACKET:  插入点的优先级小于等于当前任务的优先级*/
            if (searchPriority <= priority)
            {
                break;
            }

            /* @KEEP_COMMENT: 记录上一个节点 */
            recordNode = list_prev(recordNode, header);
        }

        /*
         *如果待插入的任务的优先级等于查找到的节点的优先级
         *则将该任务插入到同优先级队列链表中
         */
        /* @REPLACE_BRACKET:  priority == searchPriority*/
        if ((recordNode != NULL) && (priority == searchPriority))
        {
            /* @KEEP_COMMENT: 优先级相同则插入到相同优先级队列列表中 */
            list_add_tail(&(theTask->resourceNode.objectNode),
                          &searchTask->wait.samePriQueue);
        }

        else
        {
            if (recordNode != NULL)
            {
                /* @KEEP_COMMENT: 将待插入的任务节点插入到插入点后 */
                list_add_after(&(theTask->resourceNode.objectNode), recordNode);
            }
            else
            {
                list_add_head(&(theTask->resourceNode.objectNode), header);
            }
        }
    }

    else
    {
        /* @KEEP_COMMENT: 从链表头开始定位插入点*/
        recordNode = list_first(header);

        /* @KEEP_COMMENT: 遍历优先级链表中每个节点的优先级以定位插入点*/
        while (recordNode != NULL)
        {
            /* @KEEP_COMMENT: 获取当前节点对应的任务块 */
            searchTask = (T_TTOS_TaskControlBlock
                              *)(((T_TTOS_ResourceTaskNode *)(recordNode))->task);
            /* @KEEP_COMMENT:记录任务块优先级 */
            searchPriority = searchTask->taskCurPriority;

            /* @REPLACE_BRACKET: 当前任务优先级大于等于节点优先级 */
            if (searchPriority >= priority)
            {
                break;
            }

            /* @KEEP_COMMENT: 记录下一个节点 */
            recordNode = list_next(recordNode, header);
        }

        /*
         *如果待插入的任务的优先级等于查找到的节点的优先级
         *则将该任务插入到同优先级队列链表中
         */
        /* @REPLACE_BRACKET:  priority == searchPriority*/
        if ((recordNode != NULL) && (priority == searchPriority))
        {
            /* @KEEP_COMMENT: 优先级相同则插入到相同优先级队列列表中 */
            list_add_tail(&(theTask->resourceNode.objectNode),
                          &searchTask->wait.samePriQueue);
        }

        else
        {
            if (recordNode != NULL)
            {
                /* @KEEP_COMMENT: 将任务插入到节点之前 */
                list_add_before(&(theTask->resourceNode.objectNode), recordNode);
            }
            else
            {
                list_add_tail(&(theTask->resourceNode.objectNode), header);
            }
        }
    }
}
