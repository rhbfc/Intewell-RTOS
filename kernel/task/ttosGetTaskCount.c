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
 *    获取当前使用的任务总数。
 * @param[out]: taskCount: 存放任务总数
 * @return:
 *    TTOS_INVALID_ADDRESS：<taskCount>指向的地址为空。
 *    TTOS_OK：获取任务总数成功。
 * @implements: RTE_DTASK.13.1
 */
T_TTOS_ReturnCode TTOS_GetTaskCount (T_UWORD *taskCount)
{
    /* @REPLACE_BRACKET: 如果<taskCount>所指向的地址为空 */
    if (NULL == taskCount)
    {
        /* @KEEP_COMMENT: 返回TTOS_INVALID_ADDRESS */
        return (TTOS_INVALID_ADDRESS);
    }

    /* @KEEP_COMMENT: 将启动用的任务总数赋值给<taskCount>  */
    *taskCount = ttosManager.inUsedResourceNum[TTOS_OBJECT_TASK];
    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}
