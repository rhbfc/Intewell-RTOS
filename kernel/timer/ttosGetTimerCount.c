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
 *    获取当前使用的定时器总数。
 * @param[out]: timerCount: 存放定时器总数
 * @return:
 *    TTOS_INVALID_ADDRESS：<timerCount>指向的地址为NULL。
 *    TTOS_OK：获取定时器总数成功。
 * @implements: DT.2.17
 */
T_TTOS_ReturnCode TTOS_GetTimerCount (T_UWORD *timerCount)
{
    /* @REPLACE_BRACKET: 如果<timerCount>所指向的地址为空 */
    if (NULL == timerCount)
    {
        /* @KEEP_COMMENT: 返回TTOS_INVALID_ADDRESS */
        return (TTOS_INVALID_ADDRESS);
    }

    /* @KEEP_COMMENT: 将启动用的定时器总数赋值给<timerCount>  */
    *timerCount = ttosManager.inUsedResourceNum[TTOS_OBJECT_TIMER];
    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}
