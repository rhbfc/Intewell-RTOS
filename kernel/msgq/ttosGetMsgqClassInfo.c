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
 *       获取系统中消息队列的数量及标识符。
 * @param[in]  msgqAreaSize: 缓冲区最大存放数目
 * @param[out] msgqCount: 存放获取的系统消息队列总数
 * @param[out] msgqArea: 存放获取到的系统消息队列ID列表
 * @return
 *       TTOS_INVALID_ADDRESS: msgqCount为NULL或msgqArea为NULL
 *       TTOS_OK: 成功获取系统中消息队列的数量及标识符
 * @implements:  RTE_DMSGQ.3.1
 */

T_TTOS_ReturnCode TTOS_GetMsgqClassInfo (T_UWORD *msgqCount, MSGQ_ID *msgqArea,
                                         T_UWORD msgqAreaSize)
{
    /* @KEEP_COMMENT: 输出ID地址不能为空，否则返回无效地址状态值 */
    /* @REPLACE_BRACKET:  (NULL == msgqCount) ||(NULL == msgqArea) */
    if ((NULL == msgqCount) || (NULL == msgqArea))
    {
        /* @REPLACE_BRACKET:  TTOS_INVALID_ADDRESS */
        return (TTOS_INVALID_ADDRESS);
    }

    /* @KEEP_COMMENT: 获取系统中使用的消息队列 */
    *msgqCount = ttosGetIdList ((T_TTOS_ObjectCoreID *)msgqArea, msgqAreaSize,
                                TTOS_OBJECT_MSGQ);
    /* @REPLACE_BRACKET:  TTOS_OK */
    return (TTOS_OK);
}
