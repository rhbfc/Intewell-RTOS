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

/* @<MOD_HEAD */
#include <ttosBase.h>
#include <ttosInterHal.h>
#include <ttosUtils.inl>
/* @MOD_HEAD> */
/* @MODULE> */

/************************外部声明******************************/
/************************前向声明******************************/
/* 实现 */
/**
 * @brief
 *       发送消息。
 * @param[in]  msgqID:  指定消息队列ID
 * @param[in]  buffer:  存放发送消息的缓冲区地址
 * @param[in]  size:    发送消息的大小
 * @param[in]  options: 消息发送的属性设置:等待、不等待、紧急消息
 * @param[in]  ticks: 设置等待的最大时间，单位为tick
 * @return
 *       TTOS_INVALID_ADDRESS: buffer为NULL
 *       TTOS_INVALID_SIZE: size为0或size大于消息队列的最大允许消息长度
 *       TTOS_INVALID_ID: 无效的消息队列标识符
 *       TTOS_TOO_MANY:
 * 采用非等待方式时(options中包含TTOS_NO_WAIT或者ticks为0)消息不能立即发送
 *       TTOS_TIMEOUT:  发送等待超时
 *       TTOS_OBJECT_WAS_DELETED:  发送等待时，发现消息队列已经被删除
 *       TTOS_CALLED_FROM_ISR: 在中断中采用等待方式进行发送
 *       TTOS_OK: 成功发送消息
 * @implements:  RTE_DMSGQ.11.1
 */
T_TTOS_ReturnCode TTOS_SendMsgq (MSGQ_ID msgqID, T_VOID *buffer, T_UWORD size,
                                 T_UWORD options, T_UWORD ticks)
{
    T_TTOS_MsgqControlBlock *theMessageQueue;
    T_TTOS_TaskControlBlock *executing;
    T_TTOS_MsgqBufControl   *theMessage;
    struct list_node        *msgNode;
    TBSP_MSR_TYPE            msr = 0U;
    T_BOOL                   isUrgent;

    /* 从中断中调用 */
    /* @REPLACE_BRACKET: (TTOS_WAIT == options) && (TRUE == ttosIsISR()) */
    if ((TTOS_WAIT == options) && (TRUE == ttosIsISR ()))
    {
        /* @REPLACE_BRACKET: TTOS_CALLED_FROM_ISR */
        return (TTOS_CALLED_FROM_ISR);
    }

    /* 发送缓冲不能为空，否则返回无效地址状态值 */
    /* @REPLACE_BRACKET: NULL == buffer */
    if (NULL == buffer)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ADDRESS */
        return (TTOS_INVALID_ADDRESS);
    }

    /* 发送的消息大小不能够为零，否则返回无效大小状态值 */
    /* @REPLACE_BRACKET: size == U(0) */
    if (size == 0U)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_SIZE */
        return (TTOS_INVALID_SIZE);
    }

    /* @KEEP_COMMENT: 根据msgqID获取消息队列对象，对象为空返回无效ID状态值 */
    theMessageQueue = (T_TTOS_MsgqControlBlock *)ttosGetObjectById (
        msgqID, TTOS_OBJECT_MSGQ);


    /* @REPLACE_BRACKET: 消息队列为NULL */
    if (NULL == theMessageQueue)
    {

        /* @REPLACE_BRACKET: TTOS_INVALID_ID */
        return (TTOS_INVALID_ID);
    }

    /* @KEEP_COMMENT:要发送的消息不能够大于消息队列的最大允许消息 */
    /* @REPLACE_BRACKET: size > theMessageQueue->maxMsgSize */
    if (size > theMessageQueue->maxMsgSize)
    {

        /* 恢复调度 */
        (void)ttosEnableTaskDispatchWithLock ();
        /* @REPLACE_BRACKET: TTOS_INVALID_ID */
        return (TTOS_INVALID_SIZE);
    }

    /* @KEEP_COMMENT: 计算是否是紧急发送 */
    isUrgent = ((options & TTOS_URGENT) != 0U) ? TRUE : FALSE;
    /* @KEEP_COMMENT: 获取当前运行任务 */
    executing = ttosGetRunningTask ();
    /* @KEEP_COMMENT: 消息队列和发送任务都涉及中断的相关操作，因此需要关中断进行
     */
    TBSP_GLOBALINT_DISABLE (msr);
    /* @KEEP_COMMENT: 获取空闲消息块，返回值赋值给theMessage */
    msgNode = list_first (&(theMessageQueue->inactiveMsgQueue));
    theMessage = (msgNode == NULL)
                     ? NULL
                     : list_entry (msgNode, T_TTOS_MsgqBufControl, Node);
    /* @REPLACE_BRACKET: executing为空地址 */
    if (NULL == executing)
    {
        /* @KEEP_COMMENT: 使能中断进行后续的操作 */
        TBSP_GLOBALINT_ENABLE (msr);


        /* @KEEP_COMMENT: 恢复调度 */
        (void)ttosEnableTaskDispatchWithLock ();

        /* @REPLACE_BRACKET: TTOS_INVALID_ADDRESS */
        return (TTOS_INVALID_ADDRESS);
    }

    /* @KEEP_COMMENT:有空闲消息块则提交消息 */
    /* @REPLACE_BRACKET: theMessage != NULL */
    if (theMessage != NULL)
    {
        /* @KEEP_COMMENT: 设置消息优先级 */
        theMessage->priority = (T_UBYTE)TTOS_MSGQ_GET_PRIORITY (options);
        /* @KEEP_COMMENT: 从空闲消息队列中移除节点 */
        list_delete (&(theMessage->Node));
        /*
         *消息节点已经从消息队列取下，此时消息队列和当前运行任务是不允许被删除的，
         *否则消息节点就不能得到回收。
         */
        theMessageQueue->disableDeleteFlag = TRUE;

        /* @KEEP_COMMENT: 提交当前消息 */
        (void)ttosCommitMessage (theMessageQueue, theMessage, buffer, size,
                                 isUrgent, msr);


        /* @REPLACE_BRACKET: TTOS_OK */
        return (TTOS_OK);
    }

    /* @KEEP_COMMENT:检查是否不要求等待 */
    /* @REPLACE_BRACKET: ((options & TTOS_NO_WAIT) != U(0)) ||(U(0) == ticks) */
    if (((options & TTOS_NO_WAIT) != 0U) || (0U == ticks))
    {
        /* @KEEP_COMMENT: 使能中断进行后续的操作 */
        TBSP_GLOBALINT_ENABLE (msr);


        /* @KEEP_COMMENT: 恢复调度 */
        (void)ttosEnableTaskDispatchWithLock ();
        /* @REPLACE_BRACKET: TTOS_TOO_MANY */
        return (TTOS_TOO_MANY);
    }

    /* 等待对象信息*/
    /* @KEEP_COMMENT: 此处需要处理，保证event&msgq可以同时使用该结构 */
    executing->wait.id         = (T_VOID *)msgqID;
    executing->wait.returnCode = TTOS_OK;
    /* @KEEP_COMMENT: 记录发送参数信息 */
    executing->wait.returnArgument  = buffer;
    executing->wait.returnArgument1 = ((T_VOID *)(T_ULONG)size);
    executing->wait.option          = isUrgent;
    /* @KEEP_COMMENT: 设置消息优先级 */
    executing->wait.count = TTOS_MSGQ_GET_PRIORITY (options);
    (void)ttosEnqueueTaskq (&(theMessageQueue->sendWaitQueue), ticks, TRUE);
    /* @KEEP_COMMENT: 恢复中断 */
    TBSP_GLOBALINT_ENABLE (msr);


    /* @KEEP_COMMENT: 恢复调度 */
    (void)ttosEnableTaskDispatchWithLock ();
    /* 返回任务醒来的返回值 */
    /* @REPLACE_BRACKET: executing->wait.returnCode */
    return (executing->wait.returnCode);
}
