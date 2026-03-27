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

/**
 * @brief
 *      删除定时器。
 * @param[in]  timerID: 要删除的定时器标识符
 * @return
 *       TTOS_CALLED_FROM_ISR: 从中断中调用。
 *       TTOS_INVALID_ID: 无效的定时器标识符。
 *       TTOS_OK: 成功删除指定定时器。
 * @implements: RTE_DTIMER.5
 */
T_TTOS_ReturnCode TTOS_DeleteTimer (TIMER_ID timerID)
{
    TBSP_MSR_TYPE             msr = 0U;
    T_TTOS_TimerControlBlock *timerCB;

    /* @REPLACE_BRACKET: TRUE == ttosIsISR() */
    if (TRUE == ttosIsISR ())
    {
        /* @REPLACE_BRACKET: TTOS_CALLED_FROM_ISR */
        return (TTOS_CALLED_FROM_ISR);
    }

    /* @KEEP_COMMENT: 获取<timerID>指定的定时器存放至变量timerCB */
    timerCB = (T_TTOS_TimerControlBlock *)(ttosGetObjectById (
        timerID, TTOS_OBJECT_TIMER));

    /* @REPLACE_BRACKET: 指定的定时器不存在 */
    if (NULL == timerCB)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ID */
        return (TTOS_INVALID_ID);
    }
    /* @KEEP_COMMENT: 禁止中断 */
    TBSP_GLOBALINT_DISABLE (msr);

    /* @REPLACE_BRACKET: timerCB状态是TTOS_TIMER_ACTIVE */
    if (TTOS_TIMER_ACTIVE == timerCB->state)
    {
        /*
         * @KEEP_COMMENT: 调用ttosExactWaitedTimer(DT.4.8)将timerCB从定时器等待
         * 队列中移出
         */
        (void)ttosExactWaitedTimer (timerCB);
    }

    (void)ttosReturnObjectToInactiveResource (timerCB, TTOS_OBJECT_TIMER,
                                              FALSE);
    /* @KEEP_COMMENT: 使能中断 */
    TBSP_GLOBALINT_ENABLE (msr);
    (void)ttosEnableTaskDispatchWithLock ();
    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}
