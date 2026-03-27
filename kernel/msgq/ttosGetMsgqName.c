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
#include <ttosUtils.inl>
/* @MOD_HEAD> */

/************************宏 定 义******************************/
/************************类型定义******************************/
/************************外部声明******************************/
/************************前向声明******************************/
/************************模块变量******************************/
/************************全局变量******************************/
/************************实   现*******************************/

/* @MODULE> */

/**
 * @brief:
 *    获取指定消息队列的名字。
 * @param[in]: msgqID: 消息队列的ID
 * @param[out]: msgqName: 消息队列的名字
 * @return:
 *    TTOS_INVALID_ID：<msgqID>表示的对象类型不是消息队列；
 *                     <msgqID>指定的消息队列不存在。
 *    TTOS_INVALID_ADDRESS：<msgqName>指向的地址为空。
 *    TTOS_OK：获取消息队列的名字成功。
 * @implements:
 */
T_TTOS_ReturnCode TTOS_GetMsgqName (MSGQ_ID msgqID, T_UBYTE *msgqName)
{
    T_TTOS_MsgqControlBlock *msgqCB;

    /* @REPLACE_BRACKET: <semaName>为NULL */
    if (NULL == msgqName)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ADDRESS */
        return (TTOS_INVALID_ADDRESS);
    }

    /* @KEEP_COMMENT: 获取<semaID>指定的信号量存放至变量msgqCB */
    msgqCB = (T_TTOS_MsgqControlBlock *)ttosGetObjectById (msgqID,
                                                           TTOS_OBJECT_MSGQ);

    /* @REPLACE_BRACKET: sema为NULL */
    if (NULL == msgqCB)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ID */
        return (TTOS_INVALID_ID);
    }

    /* @KEEP_COMMENT:
     * 调用ttosCopyName(DT.8.2)将msgqCB消息队列的名字拷贝至<msgqName> */
    ttosCopyName (msgqCB->objCore.objName, msgqName);

    ttosEnableTaskDispatchWithLock ();

    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}
