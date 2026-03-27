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

#include <ttosBase.h>
#include <ttosInterHal.h>
#include <ttosUtils.inl>
/* @MOD_HEAD> */

/* @MODULE> */
/*实现*/
/************************外部声明******************************/
/************************前向声明******************************/
/*
 * @brief:
 *    获取等待在指定队列上的任务数量及列表
 * @param[in]   theChain: 任务队列
 * @param[out]  count:    任务队列上的任务数
 * @param[out]  idArea: 存放任务ID列表的缓存地址
 * @param[in]  areaSize: 缓存空间大小
 * @return:
 *    无
 * @implements: RTE_DMSGQ.6.1
 */
T_MODULE void ttosGetWaitedListCount (struct list_head *theChain,
                                      T_UWORD *count, TASK_ID *idArea,
                                      T_UWORD areaSize)
{
    T_UWORD           listAmount = 0U;
    struct list_node *pNode;
    TASK_ID           taskID;
    /* @KEEP_COMMENT: 获取链表头节点*/
    pNode = list_first(theChain);

    /* @REPLACE_BRACKET: pNode != NULL */
    while (pNode != NULL)
    {
        /* @REPLACE_BRACKET: idArea != NULL */
        if (idArea != NULL)
        {
            /* @KEEP_COMMENT: 判断是否达到允许的最大节点数 */
            /* @REPLACE_BRACKET: listAmount == areaSize */
            if (listAmount == areaSize)
            {
                break;
            }

            taskID  = (TASK_ID)(((T_TTOS_ResourceTaskNode *)pNode)->task);
            *idArea = taskID;
            idArea++;
        }

        listAmount++;
        /* @KEEP_COMMENT: 移动链表，获取链表下一个节点 */
        pNode = list_next(pNode, theChain);
    }

    *count = listAmount;
}

/**
 * @brief:
 *    获取指定消息队列的信息。
 * @param[in] msgqID: 消息队列标识符
 * @param[out]||[in] msgqInfo: 获取到的消息队列信息结构指针
 * @return:
 *    TTOS_INVALID_ADDRESS: msgqInfo为NULL
 * 或msgqInfo->pendingArea为NULL或msgqInfo->waitedTaskIDArea为NULL
 *    TTOS_INVALID_ID: 无效的消息队列标识符
 *    TTOS_INVALID_SIZE: 存放未决消息的空间不够(要求的未决消息空间=
 * 未决消息个数* 消息的最大长度) TTOS_OK: 成功获取指定消息队列的信息
 * @implements: RTE_DMSGQ.6.2
 */

T_TTOS_ReturnCode TTOS_GetMsgqInfo (MSGQ_ID msgqID, T_TTOS_MsgqInfo *msgqInfo)
{
    T_TTOS_MsgqControlBlock *theMessageQueue;
    T_TTOS_MsgqBufControl   *theMessage = NULL;
    struct list_node        *pNode      = NULL;
    T_UWORD                 *pendingAreaBuffer;
    T_UWORD                  i;
    T_UWORD                  taskWaitCount;
    TBSP_MSR_TYPE            msr = 0U;
    T_UWORD                  sizeRequired;
    T_UWORD                  pendingNumber;
    struct list_head        *msgQueue;

    /* @KEEP_COMMENT: 输出ID地址不能为空，否则返回无效地址状态值 */
    /* @REPLACE_BRACKET: NULL == msgqInfo */
    if (NULL == msgqInfo)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ADDRESS */
        return (TTOS_INVALID_ADDRESS);
    }

    /* @KEEP_COMMENT: 根据id获取消息队列对象，对象为空返回无效ID状态值 */
    theMessageQueue = (T_TTOS_MsgqControlBlock *)ttosGetObjectById (
        msgqID, TTOS_OBJECT_MSGQ);

    /* @REPLACE_BRACKET: 消息队列为NULL */
    if (NULL == theMessageQueue)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ID */
        return (TTOS_INVALID_ID);
    }

    /* @KEEP_COMMENT: 禁止虚拟中断 */
    TBSP_GLOBALINT_DISABLE (msr);
    /* @KEEP_COMMENT: 获取未决消息个数及未决消息 */
    msgqInfo->pendingNumber = theMessageQueue->pendingCount;
    pendingNumber           = theMessageQueue->pendingCount;
    /* @KEEP_COMMENT: 获得缓存空间 */
    pendingAreaBuffer = (T_UWORD *)msgqInfo->pendingArea;

    /* @REPLACE_BRACKET: (NULL != pendingAreaBuffer) && (U(0) !=
     * msgqInfo->pendingNumber) */
    if ((NULL != pendingAreaBuffer) && (0U != msgqInfo->pendingNumber))
    {
        sizeRequired = pendingNumber * theMessageQueue->maxMsgSize;

        /* @REPLACE_BRACKET: 如果存放未决消息的空间小于需求的空间 */
        if (msgqInfo->pendingAreaSize < sizeRequired)
        {
            /* 开启虚拟中断 */
            TBSP_GLOBALINT_ENABLE (msr);
            /* @KEEP_COMMENT: 恢复调度 */
            (void)ttosEnableTaskDispatchWithLock ();
            /* @REPLACE_BRACKET: TTOS_INVALID_SIZE */
            return (TTOS_INVALID_SIZE);
        }

        /*
         *为了保证关中断时间的确定性，拷贝消息过程中需要恢复中断
         */
        TBSP_GLOBALINT_ENABLE (msr);

        /* @KEEP_COMMENT:
         *由于中断中是允许发送消息，并且新发送的消息要么在头，要么在尾；
         *但是不允许接受消息，所以在获取消息过程中，中断处理程序新接受
         *的消息是不会返回的。
         */
        /* @REPLACE_BRACKET: i = U(0); i < TTOS_MSGQ_PRIORITY_NUMBER; i++ */
        for (i = 0U; i < TTOS_MSGQ_PRIORITY_NUMBER; i++)
        {
            /* @KEEP_COMMENT: 获取消息队列中的未决消息,返回值赋值给msgQueue */
            msgQueue = &(theMessageQueue->pendingMsgQueue[i]);

            /* @KEEP_COMMENT: 获取第一个节点，返回给pNode */
            pNode = list_first (msgQueue);

            /* @REPLACE_BRACKET: NULL != pNode */
            while (NULL != pNode)
            {
                theMessage = list_entry (pNode, T_TTOS_MsgqBufControl, Node);
                /* @KEEP_COMMENT: 将消息队列中的未决消息拷贝到缓冲中 */
                (void)ttosCopyString (pendingAreaBuffer, theMessage->buffer,
                                      theMessage->size);
                pendingAreaBuffer += theMessageQueue->maxMsgSize / 4U;
                /* @KEEP_COMMENT:  消息节点往后移动 */
                pNode = list_next (pNode, msgQueue);
            }
        }

        TBSP_GLOBALINT_DISABLE (msr);
    }

    /*
     *超时时，定时器中断处理程序中会访问此链表，
     *所以此处需要关中断访问。
     */
    /* @REPLACE_BRACKET: U(0) != theMessageQueue->pendingCount */
    if (0U != theMessageQueue->pendingCount)
    {
        /* @KEEP_COMMENT:
         * 如果存在挂起的消息，则任务接收等待队列上不可能有任务，只需要获取任务发送等待队列上的任务*/
        ttosGetWaitedListCount (
            &(theMessageQueue->sendWaitQueue.queues.fifoQueue), &taskWaitCount,
            msgqInfo->waitedTaskIDArea, msgqInfo->waitedTaskIDAreaSize);
    }

    else
    {
        /* @KEEP_COMMENT:
         * 如果不存在挂起的消息，则任务发送等待队列上不可能有任务，只需要获取任务接收等待队列上的任务*/
        ttosGetWaitedListCount (
            &(theMessageQueue->receiveWaitQueue.queues.fifoQueue),
            &taskWaitCount, msgqInfo->waitedTaskIDArea,
            msgqInfo->waitedTaskIDAreaSize);
    }

    /* @KEEP_COMMENT:获取等待在消息队列上的任务数及消息队列的名字 */
    msgqInfo->numbeOfWaited = taskWaitCount;
    (void)ttosCopyName (theMessageQueue->objCore.objName, msgqInfo->name);
    /* @KEEP_COMMENT: 获取消息队列的各项属性值 */
    msgqInfo->maxMsgSize    = theMessageQueue->maxMsgSize;
    msgqInfo->maxPendingMsg = theMessageQueue->maxPendingMsg;
    /* @KEEP_COMMENT:开启虚拟中断 */
    TBSP_GLOBALINT_ENABLE (msr);
    /* @KEEP_COMMENT:恢复调度 */
    (void)ttosEnableTaskDispatchWithLock ();
    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}
