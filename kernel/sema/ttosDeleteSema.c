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
/* @<MOD_EXTERN */

/* @MOD_EXTERN> */

/************************前向声明******************************/
/************************模块变量******************************/
/************************全局变量******************************/
/************************实   现*******************************/
/* @MODULE> */

/**
 * @brief
 *      删除信号量。
 * @param[in]  semaID: 要删除的信号量标识符
 * @return
 *       TTOS_CALLED_FROM_ISR: 从中断中调用。
 *       TTOS_INVALID_ID: 无效的信号量标识符。
 *       TTOS_RESOURCE_IN_USE: 被删除的互斥信号量忙。
 *       TTOS_OK: 成功删除指定信号量。
 * @implements: RTE_DSEMA.2.1
 */
T_TTOS_ReturnCode TTOS_DeleteSema (SEMA_ID semaID)
{
    TBSP_MSR_TYPE            msr = 0U;
    T_TTOS_SemaControlBlock *semaCB;

    /* @REPLACE_BRACKET: TRUE == ttosIsISR() */
    if (TRUE == ttosIsISR ())
    {
        /* @REPLACE_BRACKET: TTOS_CALLED_FROM_ISR */
        return (TTOS_CALLED_FROM_ISR);
    }

    /* @KEEP_COMMENT: 获取<semaID>指定的信号量存放至变量semaCB */
    semaCB = (T_TTOS_SemaControlBlock *)(ttosGetObjectById (semaID,
                                                            TTOS_OBJECT_SEMA));

    /* @REPLACE_BRACKET: 指定的信号量不存在 */
    if (NULL == semaCB)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ID */
        return (TTOS_INVALID_ID);
    }

    /* @KEEP_COMMENT: 禁止中断 */
    TBSP_GLOBALINT_DISABLE (msr);
    ttosFlushTaskq (&(semaCB->waitQueue), TTOS_OBJECT_WAS_DELETED);
    ttosReturnObjectToInactiveResource (semaCB, TTOS_OBJECT_SEMA, FALSE);
    /* @KEEP_COMMENT: 使能中断 */
    TBSP_GLOBALINT_ENABLE (msr);
    ttosEnableTaskDispatchWithLock ();
    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}
