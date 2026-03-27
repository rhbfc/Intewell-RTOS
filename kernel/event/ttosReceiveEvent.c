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

/**
 * @brief
 *       接收指定事件集,当不能立即获得事件时，根据指定的等待方式等待事件。
 * @param[in]   eventCB:   事件控制块
 * @param[in]   eventIn:   本次接收的事件条件集
 * @param[in]   optionSet: 接收事件的选项集。
 *              TTOS_EVENT_ANY: 表示接受事件集中的任意一个事件即满足条件。
 *              TTOS_EVENT_ALL: 表示接受事件集中的全部事件才满足条件。
 *              TTOS_NO_WAIT:   表示不等待,输出当前状态下能够得到的事件。
 *              TTOS_WAIT:      表示等待未被接收的事件。
 * @param[in]   ticks: 接收的事件条件不满足时指定任务的等待时间，单位为tick。
 *              0:     表示任务不等待事件；
 *              TTOS_EVENT_WAIT_FOREVER:表示任务永久等待事件
 * @param[out]  eventOut: 返回的接收事件集
 * @return
 *       TTOS_CALLED_FROM_ISR: 从中断中调用
 *       TTOS_INVALID_USER：当前任务为IDLE任务
 *       TTOS_INVALID_ADDRESS: 参数eventOut为NULL
 *       TTOS_UNSATISFIED:     条件不能满足
 *       TTOS_TIMEOUT:         接收事件超时
 *       TTOS_OK:              接受事件成功
 * @implements:  RTE_DEVENT.2.1
 */
T_TTOS_ReturnCode TTOS_ReceiveEvent (T_TTOS_EventControl *eventCB, T_TTOS_EventSet eventIn,
                                     T_TTOS_Option optionSet, T_UWORD ticks,
                                     T_TTOS_EventSet *eventOut)
{
    T_TTOS_TaskControlBlock *executing;
    T_TTOS_EventSet          seizedEvents;
    T_TTOS_EventSet          pendingEvents;
    TBSP_MSR_TYPE            msr = 0U;
    /* @KEEP_COMMENT:  调用ttosGetRunningTask() 获得当前运行任务 */
    executing = ttosGetRunningTask ();

    /* @KEEP_COMMENT: 调用ttosIsISR() 判断是否在中断中调用 */
    /* @REPLACE_BRACKET: TRUE == ttosIsISR() */
    if (TRUE == ttosIsISR ())
    {
        /* @REPLACE_BRACKET: TTOS_CALLED_FROM_ISR */
        return (TTOS_CALLED_FROM_ISR);
    }

    /* @KEEP_COMMENT: 调用ttosIsIdleTask() 判断是否是idel任务 */
    /* @REPLACE_BRACKET: TRUE == ttosIsIdleTask(executing) */
    if (TRUE == ttosIsIdleTask (executing))
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_USER */
        return (TTOS_INVALID_USER);
    }

    /* @KEEP_COMMENT: 事件输出地址为空则返回无效地址状态值 */
    /* @REPLACE_BRACKET: NULL == eventOut */
    if (NULL == eventOut)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ADDRESS */
        return (TTOS_INVALID_ADDRESS);
    }

    /* @KEEP_COMMENT:是查询当前事件则直接输出当前挂起的事件内容，并返回成功 */
    /* @REPLACE_BRACKET: eventIn == TTOS_EVENT_CURRENT */
    if (eventIn == TTOS_EVENT_CURRENT)
    {
        *eventOut = executing->wait.count;
        /* @REPLACE_BRACKET: TTOS_OK */
        return (TTOS_OK);
    }
    executing->eventCB = eventCB;
    /* @KEEP_COMMENT:调用TBSP_GLOBALINT_DISABLE_WITH_LOCK()关中断实现对发送事件的屏蔽，保障接收的原子性，因为事件发送可以在中断中被使用
     */
    TBSP_GLOBALINT_DISABLE_WITH_LOCK (msr);
    /* 获取当前的事件，并且计算当前可获取的事件值 */
    pendingEvents = eventCB->pendingEvents;
    seizedEvents  = pendingEvents & eventIn;

    /* @KEEP_COMMENT:
     * 有接收事件，并且要接收的事件等于当前可获取事件或者接收选项为任意事件，则进行事件的接收处理
     */
    /* @REPLACE_BRACKET: (seizedEvents != U(0)) &&((seizedEvents == eventIn) ||
     * ((optionSet & TTOS_EVENT_ANY) != U(0))) */
    if ((seizedEvents != 0U)
        && ((seizedEvents == eventIn) || ((optionSet & TTOS_EVENT_ANY) != 0U)))
    {
        /* 清除已接收事件 */
        eventCB->pendingEvents = pendingEvents & (~seizedEvents);
        /* @KEEP_COMMENT:
         * 调用TBSP_GLOBALINT_ENABLE_WITH_LOCK()完成了事件接收处理，因此可以使能中断进行后续的操作
         */
        TBSP_GLOBALINT_ENABLE_WITH_LOCK (msr);
        /* 输出接收到的事件 */
        *eventOut = seizedEvents;
        /* @REPLACE_BRACKET: TTOS_OK */
        return (TTOS_OK);
    }

    /* 当前挂起的事件不能够满足接收需求 */

    /* @KEEP_COMMENT:检查是否不要求等待 */
    /* @REPLACE_BRACKET: ((optionSet & TTOS_NO_WAIT) != U(0)) ||(U(0) == ticks)
     */
    if (((optionSet & TTOS_NO_WAIT) != 0U) || (0U == ticks))
    {
        /* @KEEP_COMMENT:
         * 调用TBSP_GLOBALINT_ENABLE_WITH_LOCK()使能中断进行后续的操作  */
        TBSP_GLOBALINT_ENABLE_WITH_LOCK (msr);
        /* @REPLACE_BRACKET: TTOS_UNSATISFIED */
        return (TTOS_UNSATISFIED);
    }

    /* 初置任务的返回状态值为成功 */
    executing->wait.returnCode = TTOS_OK;
    /* 设置等待参数 */
    executing->wait.option         = optionSet;
    executing->wait.count          = eventIn;
    executing->wait.returnArgument = eventOut;
    executing->wait.returnCode     = TTOS_OK;
    /* @KEEP_COMMENT:调用ttosClearTaskReady()清除当前运行任务ttosManager.runningTask的就绪状态*/
    (void)ttosClearTaskReady (executing);
    /* @REPLACE_BRACKET: <ticks>不等于TTOS_SEMA_WAIT_FOREVER */
    if (ticks != TTOS_EVENT_WAIT_FOREVER)
    {
        /* @KEEP_COMMENT:
         * 设置当前运行任务ttosManager.runningTask的等待时间为<ticks> */
        (executing)->waitedTicks = ticks;
        /* @KEEP_COMMENT:
         * 调用ttosInsertWaitedTask()将当前运行任务ttosManager.runningTask插入任务tick等待队列*/
        (void)ttosInsertWaitedTask (executing);
    }
    
    /* 设置当前任务为事件等待状态 */
    executing->state = (T_UWORD)TTOS_TASK_WAITING_FOR_EVENT;
    list_add(&executing->event_wait_node, &eventCB->wait_list);

    /* @KEEP_COMMENT:
     * 调用ttosScheduleInIntDisAndKernelLock()已完成同步状态以及相关参数的设置，可以允许事件发送中断，因此使能中断，并且恢复调度*/
    (void)ttosScheduleInIntDisAndKernelLock (msr);
    /* @KEEP_COMMENT:输出等待返回的状态值 */
    /* @REPLACE_BRACKET: executing->wait.returnCode */

    /* 超时退出 */
    if((executing->event_wait_node.next != NULL) && (executing->event_wait_node.prev != NULL))
    {
        list_del(&executing->event_wait_node);
    }
    return (executing->wait.returnCode);
}
