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
/************************实    现******************************/

/* @MODULE> */

/**
 * @brief:
 *    停止指定的定时器。
 * @param[in]: timerID: 定时器的ID
 * @return:
 *    TTOS_INVALID_ID：<timerID>指定的对象类型不是定时器；
 *                     <timerID>指定的定时器不存在。
 *    TTOS_INVALID_STATE：定时器不是激活状态。
 *    TTOS_OK：停止成功。
 * @implements: DT.4.7
 */
T_TTOS_ReturnCode TTOS_StopTimer (TIMER_ID timerID)
{
    TBSP_MSR_TYPE             msr = 0U;
    T_TTOS_TimerControlBlock *timer;

    /* @KEEP_COMMENT: 获取<timerID>指定的定时器，记录为timer */
    timer = (T_TTOS_TimerControlBlock *)ttosGetObjectById (timerID,
                                                           TTOS_OBJECT_TIMER);

    /* @REPLACE_BRACKET: timer不存在 */
    if (NULL == timer)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ID */
        return (TTOS_INVALID_ID);
    }

    /* 关闭中断进行定时器的访问互斥 */
    /* @KEEP_COMMENT: 禁止虚拟中断 */
    TBSP_GLOBALINT_DISABLE (msr);

    /* @REPLACE_BRACKET: timer状态不为TTOS_TIMER_ACTIVE */
    if (timer->state != (T_UWORD)TTOS_TIMER_ACTIVE)
    {
        /* @KEEP_COMMENT: 使能虚拟中断 */
        TBSP_GLOBALINT_ENABLE (msr);

        /* @KEEP_COMMENT: 重新使能调度 */
        (void)ttosEnableTaskDispatchWithLock ();

        /* @REPLACE_BRACKET: TTOS_INVALID_STATE */
        return (TTOS_INVALID_STATE);
    }

    /* @KEEP_COMMENT:
     * 调用ttosExactWaitedTimer(DT.4.8)将定时器从定时器等待队列中移出 */
    (void)ttosExactWaitedTimer (timer);

    /* @KEEP_COMMENT: 设置timer状态为TTOS_TIMER_STOP */
    timer->state = (T_UWORD)TTOS_TIMER_STOP;

    /* @KEEP_COMMENT: 使能虚拟中断 */
    TBSP_GLOBALINT_ENABLE (msr);

    /* @KEEP_COMMENT: 重新使能调度 */
    (void)ttosEnableTaskDispatchWithLock ();

    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}
