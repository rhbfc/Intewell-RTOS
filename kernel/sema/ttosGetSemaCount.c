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

/* @<MOD_EXTERN */

/* @MOD_EXTERN> */

/************************前向声明******************************/
/************************模块变量******************************/
/************************全局变量******************************/
/************************实   现*******************************/

/* @MODULE> */

/**
 * @brief:
 *    获取当前使用的信号量总数。
 * @param[out]: semaCount: 存放信号量总数
 * @return:
 *    TTOS_INVALID_ADDRESS：<semaCount>指向的地址为空。
 *    TTOS_OK：获取信号量总数成功。
 * @implements: RTE_DSEMA.4.1
 */

T_TTOS_ReturnCode TTOS_GetSemaCount (T_UWORD *semaCount)
{
    /* @REPLACE_BRACKET: 如果<semaCount>所指向的地址为空 */
    if (NULL == semaCount)
    {
        /* @KEEP_COMMENT: 返回TTOS_INVALID_ADDRESS */
        return (TTOS_INVALID_ADDRESS);
    }

    /* @KEEP_COMMENT: 将启动用的任务总数赋值给<semaCount>  */
    *semaCount = ttosManager.inUsedResourceNum[TTOS_OBJECT_SEMA];
    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}
