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

/* 头文件 */
/* @<MODULE */

/* @<MOD_HEAD */
#include <stdlib.h>
#include <ttosBase.h>
#include <ttosInterHal.h>
#include <ttosUtils.inl>
/* @MOD_HEAD> */

/* @MODULE> */
/************************外部声明******************************/
T_EXTERN void ttosDisableTaskDispatchWithLock(void);
/************************前向声明******************************/

/* 实现 */

/*
 * @brief:
 *     删除消息发送等待以及接收等待队列上的任务。
 * @param[in]  theMessageQueue: 消息队列控制块
 * @return:
 *    无
 * @implements:  RTE_DMSGQ.2.1
 */
T_MODULE void ttosDeleteWaitingTask(T_TTOS_MsgqControlBlock *theMessageQueue)
{
    UINT32 status = (UINT32)TTOS_OBJECT_WAS_DELETED;
    /* @KEEP_COMMENT:
     * 如果sendWaitQueue等待队列不为空，那么将等待队列中的任务移除，并清除等待态
     */
    //  (void)ttosFlushTaskq(&(theMessageQueue->sendWaitQueue),
    //  TTOS_OBJECT_WAS_DELETED);
    (void)ttosFlushTaskq(&(theMessageQueue->sendWaitQueue), status);
    /* @KEEP_COMMENT:
     * 如果receiveWaitQueue等待队列不为空，那么将等待队列中的任务移除，并清除等待态
     */
    status = (UINT32)TTOS_OBJECT_WAS_DELETED;
    (void)ttosFlushTaskq(&(theMessageQueue->receiveWaitQueue), status);
    //(void)ttosFlushTaskq(&(theMessageQueue->receiveWaitQueue),
    // TTOS_OBJECT_WAS_DELETED);
}

/*
 * @brief:
 *    删除消息。
 * @param[in] msgQueue: 要删除的消息所属队列
 * @return:
 *    无
 * @implements:  RTE_DMSGQ.2.2
 */
T_MODULE void ttosDeleteMessageFromQueue(struct list_head *msgQueue)
{
    struct list_node *pNode;

    /* @REPLACE_BRACKET:  FALSE == ret_bool */
    while (!list_is_empty(msgQueue))
    {
        pNode = list_first(msgQueue);
        /* @KEEP_COMMENT: 从msgQueue消息队列中移除消息节点 */
        list_delete(pNode);
    }
}

/*
 * @brief:
 *    删除消息。
 * @param[in]  theMessageQueue: 要删除的消息队列控制块
 * @return:
 *    无
 * @implements: RTE_DMSGQ.2.3
 */
void ttosDeleteMessage(T_TTOS_MsgqControlBlock *theMessageQueue)
{
    T_UWORD i;

    /* @KEEP_COMMENT:
     * 如果pending_messages链表不为空，那么将pending_message的消息节点归还到系统消息节点链表
     */
    /* @REPLACE_BRACKET:  i = U(0); i < TTOS_MSGQ_PRIORITY_NUMBER; i++ */
    for (i = 0U; i < TTOS_MSGQ_PRIORITY_NUMBER; i++)
    {
        /* @KEEP_COMMENT:
         * 如果Inactive_messages链表不为空，那么将pendingMsgQueue的消息节点归还到系统消息节点链表
         */
        ttosDeleteMessageFromQueue(&(theMessageQueue->pendingMsgQueue[i]));
    }

    /* @KEEP_COMMENT:
     * 如果Inactive_messages链表不为空，那么将Inactive_messages的消息节点归还到系统消息节点链表
     */
    ttosDeleteMessageFromQueue(&(theMessageQueue->inactiveMsgQueue));

    /* @KEEP_COMMENT: 释放消息队列的消息节点缓存资源 */
    if (theMessageQueue->msgBuffers != NULL)
    {
        free(theMessageQueue->msgBuffers);
    }
}

/**
 * @brief:
 *    删除消息队列。
 * @param[in] msgqID: 要删除的消息队列标识符
 * @return:
 *    TTOS_CALLED_FROM_ISR: 从中断中调用
 *    TTOS_INVALID_ID: 无效的消息队列标识符
 *    TTOS_RESOURCE_IN_USE: 指定的消息队列正在拷贝消息
 *    TTOS_OK: 成功删除指定消息队列
 * @implements: RTE_DMSGQ.2.4
 */

T_TTOS_ReturnCode TTOS_DeleteMsgq(MSGQ_ID msgqID)
{
    T_TTOS_MsgqControlBlock *theMessageQueue;

    /* @REPLACE_BRACKET:  TRUE == ttosIsISR() */
    if (TRUE == ttosIsISR())
    {
        /* @REPLACE_BRACKET: TTOS_CALLED_FROM_ISR */
        return (TTOS_CALLED_FROM_ISR);
    }

    /* @KEEP_COMMENT: 根据msgqID获取消息队列对象，对象为空返回无效ID状态值 */
    theMessageQueue = (T_TTOS_MsgqControlBlock *)ttosGetObjectById(msgqID, TTOS_OBJECT_MSGQ);

    /* @REPLACE_BRACKET:  theMessageQueue == NULL */
    if (theMessageQueue == NULL)
    {
        /* @REPLACE_BRACKET:  TTOS_INVALID_ID */
        return (TTOS_INVALID_ID);
    }

    /* @REPLACE_BRACKET:  TRUE == theMessageQueue->disableDeleteFlag */
    if (TRUE == theMessageQueue->disableDeleteFlag)
    {
        (void)ttosEnableTaskDispatchWithLock();
        /* @REPLACE_BRACKET:  TTOS_RESOURCE_IN_USE */
        return (TTOS_RESOURCE_IN_USE);
    }

    /* @KEEP_COMMENT:
     * 将对象控制块从系统正在使用的对象资源队列中删除，这样就接下来就可以最小范围内的禁止调度和中断。*/
    list_delete(&(theMessageQueue->objCore.objResourceNode.objectResourceNode));
    /* @KEEP_COMMENT: 恢复调度 */
    (void)ttosEnableTaskDispatchWithLock();
    /* @KEEP_COMMENT: 删除消息队列中的消息节点 */
    ttosDeleteMessage(theMessageQueue);
    /* @KEEP_COMMENT: 删除消息发送等待以及接收等待队列上的任务 */
    ttosDeleteWaitingTask(theMessageQueue);
    /* @KEEP_COMMENT: 禁止调度 */
    (void)ttosDisableTaskDispatchWithLock();
    /* @KEEP_COMMENT: 归还消息队列控制块资源 */
    (void)ttosReturnObjectToInactiveResource(theMessageQueue, TTOS_OBJECT_MSGQ, TRUE);
    /* @KEEP_COMMENT: 恢复调度 */
    (void)ttosEnableTaskDispatchWithLock();
    /* @REPLACE_BRACKET:  TTOS_OK */
    return (TTOS_OK);
}
