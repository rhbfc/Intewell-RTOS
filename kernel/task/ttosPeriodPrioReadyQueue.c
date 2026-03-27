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

#include <commonTypes.h>
#include <ttos.h>
#include <ttosBase.h>

T_MODULE struct list_head ttosPeriodPrioReadyQueueHead;
T_MODULE T_UBYTE isInited;

T_VOID ttosPeriodPrioReadyQueueAdd (T_TTOS_TaskControlBlock *task) 
{
    T_TTOS_TaskControlBlock *taskNode;

    if (!isInited)
    {
        isInited = 1;
        INIT_LIST_HEAD(&ttosPeriodPrioReadyQueueHead);
    }

    struct list_head *head = &ttosPeriodPrioReadyQueueHead;

    taskNode = (T_TTOS_TaskControlBlock *)list_first(head);
    
    while (taskNode)
    {
       
        if (task->periodNode.delayTime < taskNode->periodNode.delayTime)
        {
            list_add_before(&task->objCore.objectNode, &taskNode->objCore.objectNode);
            return;
        }
        else
        {
            taskNode = (T_TTOS_TaskControlBlock *)list_next((struct list_node *)taskNode, head);
        }   
    }

    list_add_tail(&task->objCore.objectNode, head);
}

T_VOID ttosPeriodPrioReadyQueueSetReady (T_VOID) 
{
    T_TTOS_TaskControlBlock *taskNode;
    T_TTOS_TaskControlBlock *nextNode;
    struct list_head     *head = &ttosPeriodPrioReadyQueueHead;

    if (!isInited)
    {
        isInited = 1;
        INIT_LIST_HEAD(&ttosPeriodPrioReadyQueueHead);
    }

    taskNode = (T_TTOS_TaskControlBlock *)list_first(head);

    while (taskNode)
    {
        nextNode = (T_TTOS_TaskControlBlock *)list_next((struct list_node *)taskNode, head);
        list_delete(&taskNode->objCore.objectNode);
        ttosSetTaskReady(taskNode);
        taskNode = nextNode;
    }
}
