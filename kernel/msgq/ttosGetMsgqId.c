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
#include <ttosBase.h>
#include <ttosInterHal.h>
#include <ttosUtils.inl>

/* ttos控制结构 */
T_EXTERN T_TTOS_TTOSManager ttosManager;

/* 实现 */

/**
 * @brief
 *       根据消息队列名获取标识符。
 * @param[in]  msgqName: 消息队列名
 * @param[out] msgqID: 获取到的消息队列标识符
 * @return
 *       TTOS_INVALID_ADDRESS:  消息队列名字或者消息队列标识符地址为NULL
 *       TTOS_INVALID_NAME:       返回消息队列名不合法
 *       TTOS_OK: 成功获取消息队列标识符
 * @implements  DC.6.29
 */
T_TTOS_ReturnCode TTOS_GetMsgqID (T_UBYTE *msgqName, MSGQ_ID *msgqID)
{
    T_TTOS_ObjectCoreID returnObjectCoreID;

    /* 判断名字和ID地址是否为空 */
    if ((NULL == msgqName) || (NULL == msgqID))
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ADDRESS */
        return (TTOS_INVALID_ADDRESS);
    }

    returnObjectCoreID = ttosGetIdByName (msgqName, TTOS_OBJECT_MSGQ);
    if (NULL == returnObjectCoreID)
    {
        /* 返回消息队列名不合法 */
        return (TTOS_INVALID_NAME);
    }

    *msgqID = (MSGQ_ID)returnObjectCoreID;

    /* 成功返回 */
    return (TTOS_OK);
}
