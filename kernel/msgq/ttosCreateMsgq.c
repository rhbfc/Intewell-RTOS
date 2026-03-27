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

/* @<MOD_HEAD */
#include <stdlib.h>
#include <ttosBase.h>
#include <ttosInterHal.h>
#include <ttosUtils.inl>
/************************外部声明******************************/
/* @MOD_HEAD> */
T_EXTERN void ttosDisableTaskDispatchWithLock(void);
/* @MODULE> */

/************************前向声明******************************/

/* 实现 */

/*
 * @brief:
 *    创建消息队列的消息节点
 * @param[in]  theMessageQueue: 消息队列控制对象
 * @param[in]  count:要分配的消息数
 * @return:
 *    TTOS_UNSATISFIED: 系统分配资源失败
 *    TTOS_OK: 成功创建消息节点
 * @implements:  RTE_DMSGQ.1.1
 */
T_MODULE T_TTOS_ReturnCode ttosCreateMessageNode(T_TTOS_MsgqControlBlock *theMessageQueue,
                                                 T_UWORD count)
{
    T_UWORD i;
    T_UWORD blockSize;
    T_UWORD totalSize;
    T_UBYTE *msgBufferBase;
    T_TTOS_MsgqBufControl *theMessage;

    blockSize = theMessageQueue->maxMsgSize + sizeof(T_TTOS_MsgqBufControl) - sizeof(T_UWORD);
    blockSize = (blockSize + TTOS_OBJECT_MALLOC_DEFAULT_SIZE - 1U) &
                ~(TTOS_OBJECT_MALLOC_DEFAULT_SIZE - 1U);
    totalSize = blockSize * count;

    /* @KEEP_COMMENT:根据消息控制块的大小和数量统一分配内存空间 */
    theMessageQueue->msgBuffers =
        (T_TTOS_MsgqBufControl *)memalign(TTOS_OBJECT_MALLOC_DEFAULT_SIZE, totalSize);
    if (theMessageQueue->msgBuffers == NULL)
    {
        /* @REPLACE_BRACKET:  TTOS_UNSATISFIED */
        return (TTOS_UNSATISFIED);
    }
    memset(theMessageQueue->msgBuffers, 0, totalSize);
    msgBufferBase = (T_UBYTE *)theMessageQueue->msgBuffers;
    /* @KEEP_COMMENT: 将所有的消息块都插入到空闲消息队列 */
    /* @REPLACE_BRACKET:  i = U(0); i < count ; i++ */
    for (i = 0U; i < count; i++)
    {
        theMessage = (T_TTOS_MsgqBufControl *)(msgBufferBase + (blockSize * i));

        /* @REPLACE_BRACKET:  theMessage == NULL */
        if (theMessage == NULL)
        {
            /* @REPLACE_BRACKET:  TTOS_UNSATISFIED */
            return (TTOS_UNSATISFIED);
        }

        theMessage->size = 0U;
        /* @KEEP_COMMENT:消息节点插入到空闲消息队列的队尾 */
        list_add_tail(&(theMessage->Node), &(theMessageQueue->inactiveMsgQueue));
    }

    /* @REPLACE_BRACKET:  TTOS_OK */
    return (TTOS_OK);
}

/*
 * @brief:
 *    分配消息队列的消息节点
 * @param[in]  theMessageQueue: 消息队列控制对象
 * @return:
 *    TTOS_UNSATISFIED: 系统分配资源失败
 *    TTOS_OK: 成功创建消息节点
 * @implements: RTE_DMSGQ.1.2
 */
T_MODULE T_TTOS_ReturnCode ttosAllocMessageNode(T_TTOS_MsgqControlBlock *theMessageQueue)
{
    T_TTOS_ReturnCode ret;
    /* @KEEP_COMMENT:从系统资源中创建消息块 */
    ret = ttosCreateMessageNode(theMessageQueue, theMessageQueue->maxPendingMsg);
    /* @REPLACE_BRACKET: ret */
    return (ret);
}

/*
 * @brief:
 *    初始化消息队列。
 * @param[out]||[in]: msgqCB: 消息队列控制块
 * @param[in]: msgqConfig: 任务配置参数
 * @param[out] msgqID: 创建的消息队列标识符
 * @return:
 *    TTOS_UNSATISFIED: 分配消息队列资源失败
 *    TTOS_OK:          成功创建消息队列
 * @tracedREQ:
 * @implements: RTE_DMSGQ.1.3
 */
T_TTOS_ReturnCode ttosInitMsgq(T_TTOS_MsgqControlBlock *msgqCB, T_TTOS_ConfigMsgq *msgqConfig,
                               MSGQ_ID *msgqID)
{
    T_TTOS_ReturnCode ret;
    T_UWORD i;
    /* @KEEP_COMMENT:消息队列此时未插入链表 */
    msgqCB->objCore.objectNode.next = NULL;
    /* 根据配置表的属性初始化第i个消息队列 */
    msgqCB->msgqId = (MSGQ_ID)msgqCB;
    /* 初始化消息队列名字  */
    (void)ttosCopyName(msgqConfig->cfgMsgqName, msgqCB->objCore.objName);
    /* @KEEP_COMMENT:初始化消息队列参数 */
    msgqCB->attributeSet = msgqConfig->attributeSet;
    msgqCB->maxPendingMsg = msgqConfig->maxMsgqNumber;
    msgqCB->maxMsgSize = msgqConfig->maxMsgSize;
    msgqCB->pendingCount = TTOS_INIT_COUNT;
    msgqCB->disableDeleteFlag = FALSE;
    msgqCB->msgPriorityBitMap = 0U;

    msgqCB->notifyHandler = NULL;
    msgqCB->mqc.msgqID = msgqCB;
    msgqCB->mqc.notify_tid = 0;
    memset(&msgqCB->mqc.notify_sigevent, 0, sizeof(struct sigevent));

    /* @KEEP_COMMENT:初始化消息队列的消息等待队列 */
    (void)ttosInitializeTaskq(&(msgqCB->sendWaitQueue),
                              (((T_UWORD)msgqCB->attributeSet & TTOS_PRIORITY) != 0U)
                                  ? T_TTOS_QUEUE_DISCIPLINE_PRIORITY
                                  : T_TTOS_QUEUE_DISCIPLINE_FIFO,
                              (T_UWORD)TTOS_TASK_WAITING_FOR_MSGQ);
    (void)ttosInitializeTaskq(&(msgqCB->receiveWaitQueue),
                              (((T_UWORD)msgqCB->attributeSet & TTOS_PRIORITY) != 0U)
                                  ? T_TTOS_QUEUE_DISCIPLINE_PRIORITY
                                  : T_TTOS_QUEUE_DISCIPLINE_FIFO,
                              (T_TTOS_TaskState)((T_UWORD)TTOS_TASK_WAITING_FOR_MSGQ));

    /* @KEEP_COMMENT: 初始化消息队列消息优先级队列 */
    /* @REPLACE_BRACKET:  i = U(0); i < TTOS_MSGQ_PRIORITY_NUMBER; i++ */
    for (i = 0U; i < TTOS_MSGQ_PRIORITY_NUMBER; i++)
    {
        /* @KEEP_COMMENT: 初始化链表为只有控制节点的空链表 */
        INIT_LIST_HEAD(&(msgqCB->pendingMsgQueue[i]));
    }

    INIT_LIST_HEAD(&(msgqCB->inactiveMsgQueue));
    /* @KEEP_COMMENT: 分配消息空间 */
    ret = ttosAllocMessageNode(msgqCB);

    /* @REPLACE_BRACKET:  ret != TTOS_OK */
    if (ret != TTOS_OK)
    {
        /* @REPLACE_BRACKET:  TTOS_UNSATISFIED */
        return (TTOS_UNSATISFIED);
    }

    (void)ttosDisableTaskDispatchWithLock();
    /* @KEEP_COMMENT: 对象资源节点记录对应的对象 */
    (void)ttosInitObjectCore(msgqCB, TTOS_OBJECT_MSGQ);

    /* @REPLACE_BRACKET: NULL != msgqID */
    if (NULL != msgqID)
    {
        /* @KEEP_COMMENT: 设置消息队列控制块的消息队列ID */
        *msgqID = msgqCB->msgqId;
    }

    (void)ttosEnableTaskDispatchWithLock();
    /* @REPLACE_BRACKET:  TTOS_OK */
    return (TTOS_OK);
}

/*
 * @brief:
 *    创建消息队列。
 * @param[in] msgqParam  msgq参数结构，包含如下信息
 *             name: 待创建的消息队列名字
 *             count:最大消息数
 *             maxMessageSize: 消息的最大长度
 * @param[out] msgqID: 创建的消息队列标识符
 * @return:
 *    TTOS_CALLED_FROM_ISR: 从中断中调用
 *    TTOS_INVALID_ADDRESS: msgqParam为NULL,msgqID为空
 *    TTOS_INVAILD_VERSION:版本不一致
 *    TTOS_INVALID_NUMBER: msgqParam->maxMsgqNumber为0
 *    TTOS_INVALID_SIZE: msgqParam->maxMsgSize为0
 *    TTOS_TOO_MANY:
 * 系统中正在使用的消息队列数已达到用户配置的最大消息队列数(静态配置的消息队列数+
 * 可创建的消息队列数) TTOS_UNSATISFIED: 系统不能成功分配的消息队列需要的空间；
 *                       分配消息队列对象失败。
 *    TTOS_INVALID_NAME:
 * 对象名name字符串中存在ASCII取值范围不在[32,126]之间的字符
 *                                    或name为空串或者空指针。
 *    TTOS_OK: 成功创建消息队列
 * @implements:  RTE_DMSGQ.1.4
 */
T_TTOS_ReturnCode TTOS_CreateMsgq(T_TTOS_ConfigMsgq *msgqParam, MSGQ_ID *msgqID)
{
    T_TTOS_MsgqControlBlock *theMessageQueue = NULL;
    T_TTOS_ReturnCode ret;
    T_BOOL status;
    T_UWORD compret = 0U;

    /* @REPLACE_BRACKET: TRUE == ttosIsISR() */
    if (TRUE == ttosIsISR())
    {
        /* @REPLACE_BRACKET: TTOS_CALLED_FROM_ISR */
        return (TTOS_CALLED_FROM_ISR);
    }

    /* @REPLACE_BRACKET: NULL == msgqParam */
    if (NULL == msgqParam)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ADDRESS */
        return (TTOS_INVALID_ADDRESS);
    }

    /* @REPLACE_BRACKET: 比较两对象版本 */
    if (FALSE ==
        ttosCompareVerison(msgqParam->objVersion, (T_UBYTE *)TTOS_OBJ_CONFIG_CURRENT_VERSION))
    {
        /* @REPLACE_BRACKET: TTOS_INVAILD_VERSION */
        return (TTOS_INVAILD_VERSION);
    }

    /* @KEEP_COMMENT:消息队列的对象ID返回地址为空则返回无效地址状态值 */
    /* @REPLACE_BRACKET: NULL == msgqID */
    if (NULL == msgqID)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ADDRESS */
        return (TTOS_INVALID_ADDRESS);
    }

    /* @KEEP_COMMENT: 允许的最大消息数不能够为零，否则返回无效数值 */
    /* @REPLACE_BRACKET: msgqParam->maxMsgqNumber == compret */
    if (msgqParam->maxMsgqNumber == compret)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_NUMBER */
        return (TTOS_INVALID_NUMBER);
    }

    /* @KEEP_COMMENT:最大消息不能够为零，否则返回无效大小状态值 */
    /* @REPLACE_BRACKET: msgqParam->maxMsgSize == compret */
    if (msgqParam->maxMsgSize == compret)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_SIZE */
        return (TTOS_INVALID_SIZE);
    }

    /* @KEEP_COMMENT: 判断名字字符串是否符合命名规范 */
    status = ttosVerifyName(msgqParam->cfgMsgqName, TTOS_OBJECT_NAME_LENGTH);

    /* @REPLACE_BRACKET:  status == FALSE */
    if (status == FALSE)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_NAME */
        return (TTOS_INVALID_NAME);
    }

    /* @KEEP_COMMENT: 关闭调度 */
    (void)ttosDisableTaskDispatchWithLock();
    /* @KEEP_COMMENT: 调用ttosAllocateObject返回值赋值给ret */
    ret = ttosAllocateObject(TTOS_OBJECT_MSGQ, (T_VOID **)&theMessageQueue);

    /* @REPLACE_BRACKET:  TTOS_OK != ret */
    if (TTOS_OK != ret)
    {
        (void)ttosEnableTaskDispatchWithLock();
        /* @REPLACE_BRACKET: ret */
        return (ret);
    }

    (void)ttosEnableTaskDispatchWithLock();

    /* @KEEP_COMMENT: 调用ttosInitMsgq函数，返回值赋值给ret */
    ret = ttosInitMsgq(theMessageQueue, msgqParam, msgqID);

    /* @REPLACE_BRACKET: ret != TTOS_OK */
    if (ret != TTOS_OK)
    {
        /* @KEEP_COMMENT:释放已经分配的消息节点资源和消息节点缓存资源 */
        (void)ttosDeleteMessage(theMessageQueue);
        (void)ttosDisableTaskDispatchWithLock();
        /* @KEEP_COMMENT:归还消息队列控制块到系统空闲消息队列控制块列表
         * 。没有成功初始化消息队列时，消息队列控制块没有在系统正在使用的对象资源队列中。*/
        (void)ttosReturnObjectToInactiveResource(theMessageQueue, TTOS_OBJECT_MSGQ, TRUE);
        (void)ttosEnableTaskDispatchWithLock();
        /* @REPLACE_BRACKET: TTOS_UNSATISFIED */
        return (TTOS_UNSATISFIED);
    }

    /* @REPLACE_BRACKET:  TTOS_OK */
    return (TTOS_OK);
}
