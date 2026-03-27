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
/* 头文件 */
/* @<MOD_HEAD */
#include <ttosInterHal.h>
#include <ttosUtils.inl>
/* @MOD_HEAD> */

/* @MODULE> */

/************************外部声明******************************/
/************************前向声明******************************/

/* 实现 */
/**
 * @brief
 *       向任务发送事件。
 * @param[in]  eventCB: 事件控制块
 * @param[in]  eventIn: 发送事件集
 * @return
 *       TTOS_INVALID_NUMBER: 发送事件为空
 *       TTOS_INVALID_ID: 无效任务ID
 *       TTOS_OK: 发送成功
 * @implements:  RTE_DEVENT.3.1
 */
T_TTOS_ReturnCode TTOS_SendEvent (T_TTOS_EventControl *eventCB,
                                  T_TTOS_EventSet      eventIn)
{
    T_TTOS_TaskControlBlock *theTask;
    T_TTOS_TaskControlBlock *next;
    T_TTOS_Option            optionSet;
    T_TTOS_EventSet          clearEvent = 0;
    T_UWORD                  schedFlag  = 0;

    /* @KEEP_COMMENT:发送的事件为空直接返回无效值的状态值 */
    /* @REPLACE_BRACKET: eventIn == U(0) */
    if (eventIn == 0U)
    {

        /* @REPLACE_BRACKET: TTOS_INVALID_NUMBER */
        return (TTOS_INVALID_NUMBER);
    }

    eventCB->pendingEvents |= eventIn;

    ttosDisableTaskDispatchWithLock ();

    /* 遍历所有任务 */
    list_for_each_entry_safe (theTask, next, &eventCB->wait_list,
                              event_wait_node)
    {
        schedFlag = 0;

        /* 获取任务的等待选项 */
        optionSet = theTask->wait.option;
        if (optionSet & TTOS_EVENT_ANY)
        {
            if (theTask->wait.count & eventCB->pendingEvents)
            {
                clearEvent |= (theTask->wait.count & eventCB->pendingEvents);
                schedFlag = 1;
            }
        }
        else
        {
            /* TTOS_EVENT_ALL */
            if ((theTask->wait.count & eventCB->pendingEvents)
                == theTask->wait.count)
            {
                clearEvent |= (theTask->wait.count & eventCB->pendingEvents);
                schedFlag = 1;
            }
        }

        if (schedFlag)
        {


            /* @REPLACE_BRACKET: node任务在任务tick等待队列上 */
            if (theTask->objCore.objectNode.next != NULL)
            {
                /* @KEEP_COMMENT:
                 * 调用ttosExactWaitedTask(DT.2.24)将node任务从任务tick等待队列中移除
                 */
                ttosExactWaitedTask (theTask);
            }

            /* 设置task的返回值 */
            theTask->wait.returnCode = TTOS_OK;
            if (theTask->wait.returnArgument)
            {
                *(T_TTOS_EventSet *)theTask->wait.returnArgument
                    = (theTask->wait.count & eventCB->pendingEvents);
            }

            /* 清除等待的选项和条件参数  */
            theTask->wait.option = 0U;
            theTask->wait.count  = 0U;

            /* @KEEP_COMMENT:
             * 调用ttosClearTaskWaiting(DT.2.22)清除task任务的等待状态 */
            ttosClearTaskWaiting (theTask);

            /* 将任务从链表中删除 */
            list_del (&theTask->event_wait_node);
        }
    }

    if (clearEvent)
    {
        eventCB->pendingEvents &= ~clearEvent;
    }


    /* @KEEP_COMMENT: 调用ttosEnableTaskDispatchWithLock() 恢复调度 */
    (void)ttosEnableTaskDispatchWithLock ();
    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}
