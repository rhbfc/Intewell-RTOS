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
#include <ttosInterHal.h>
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

/* 实现 */

/**
 * @brief
 *       获取系统中定时器id列表。
 * @param[out] idList:   存放获取到的定时器ID列表
 * @param[in]  maxIdNum: 获取到的定时器ID数量
 * @return
 *       TTOS_INVALID_ADDRESS: idList为NULL
 *       TTOS_OK: 成功获取系统中定时器ID的数量及标识符
 */
T_TTOS_ReturnCode TTOS_GetTimerIdList (TIMER_ID idList[], T_UWORD maxIdNum)
{
    /* @KEEP_COMMENT: 输出ID地址不能为空，否则返回无效地址状态值 */
    /* @REPLACE_BRACKET: (NULL == idList) */
    if ((NULL == idList))
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ADDRESS */
        return (TTOS_INVALID_ADDRESS);
    }

    /* @KEEP_COMMENT:  获取系统中使用的定时器队列 */
    (T_VOID) ttosGetIdList ((T_TTOS_ObjectCoreID *)idList, maxIdNum,
                            TTOS_OBJECT_TIMER);
    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}
