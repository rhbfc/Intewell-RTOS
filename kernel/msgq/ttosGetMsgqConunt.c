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

/************************头 文 件******************************/
/* @<MODULE */
/* @<MOD_HEAD */
#include <ttosBase.h>
#include <ttosUtils.inl>
/* @MOD_HEAD> */
/* @MODULE> */

/************************宏 定 义******************************/
/************************类型定义******************************/
/************************外部声明******************************/

/************************前向声明******************************/
/************************模块变量******************************/
/************************全局变量******************************/
/************************实   现*******************************/

/**
 * @brief:
 *    获取系统中消息队列的数量。
 * @param[out]: msgqCount: 存放消息队列数量。
 * @return:
 *    TTOS_INVALID_ADDRESS：<msgqCount>指向的地址为空。
 *    TTOS_OK：获取消息队列总数成功。
 * @implements:  RTE_DMSGQ.4.1
 */

T_TTOS_ReturnCode TTOS_GetMsgqCount (T_UWORD *msgqCount)
{
    /* @REPLACE_BRACKET: 如果<msgqCount>所指向的地址为空 */
    if (NULL == msgqCount)
    {
        /* @KEEP_COMMENT: TTOS_INVALID_ADDRESS */
        return (TTOS_INVALID_ADDRESS);
    }

    /* @KEEP_COMMENT: 将系统中消息队列的数量赋值给<taskCount>  */
    *msgqCount = ttosManager.inUsedResourceNum[TTOS_OBJECT_MSGQ];
    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}
