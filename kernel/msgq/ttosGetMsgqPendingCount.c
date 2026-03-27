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
 *       获取指定消息队列中的消息数。
 * @param[in]  msgqID: 消息队列标识符
 * @param[out] count: 获取到的消息队列消息数的指针
 * @return
 *       TTOS_INVALID_ADDRESS: count为NULL
 *       TTOS_INVALID_ID: 无效的消息队列标识符
 *       TTOS_OK: 成功获取指定消息队列的信息
 * @implements:  RTE_DMSGQ.8.1
 */

T_TTOS_ReturnCode TTOS_GetMsgqPendingCount (MSGQ_ID msgqID, T_UWORD *count)
{
    T_TTOS_MsgqControlBlock *theMessageQueue;
    TBSP_MSR_TYPE            msr = 0U;

    /* 输出消息个数地址不能为空，否则返回无效地址状态值 */
    /* @REPLACE_BRACKET: NULL == count */
    if (NULL == count)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ADDRESS */
        return (TTOS_INVALID_ADDRESS);
    }

    /* @KEEP_COMMENT:  根据id获取消息队列对象，对象为空返回无效ID状态值 */
    theMessageQueue = (T_TTOS_MsgqControlBlock *)ttosGetObjectById (
        msgqID, TTOS_OBJECT_MSGQ);

    /* @REPLACE_BRACKET: 消息队列为NULL */
    if (NULL == theMessageQueue)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ID */
        return (TTOS_INVALID_ID);
    }

    /* @KEEP_COMMENT:  禁止虚拟中断 */
    TBSP_GLOBALINT_DISABLE (msr);
    *count = theMessageQueue->pendingCount;
    /* @KEEP_COMMENT:  开启虚拟中断 */
    TBSP_GLOBALINT_ENABLE (msr);
    /* @KEEP_COMMENT:  恢复调度 */
    (void)ttosEnableTaskDispatchWithLock ();
    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}
