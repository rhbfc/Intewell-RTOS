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

/* @<MOD_INFO */
/*
 * @file: DC_RT_4.pdl
 * @brief:
 *    <li>定时器模块主要实现软定时器的功能，在硬件定时器的驱动下，</li>
 *    <li>可以模拟若干个定时器，实现对一些计时功能的操作。</li>
 * @implements: DT.4
 */
/* @MOD_INFO> */

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
 *    激活指定的定时器，已经被激活的定时器则重新激活。
 * @param[in]: timerID: 定时器的ID
 * @return:
 *    TTOS_INVALID_ID：<timerID>指定的对象类型不是定时器；
 *                     <timerID>指定的定时器不存在。
 *    TTOS_INVALID_STATE：定时器还未被安装。
 *    TTOS_OK：激活成功。
 * @implements: DT.4.1
 */
T_TTOS_ReturnCode TTOS_ActiveTimer (TIMER_ID timerID)
{
    TBSP_MSR_TYPE             msr = 0U;
    T_TTOS_TimerControlBlock *timer;

    /* @KEEP_COMMENT: 获取<timerID>指定的定时器记录为timer */
    timer = (T_TTOS_TimerControlBlock *)(ttosGetObjectById (timerID,
                                                            TTOS_OBJECT_TIMER));

    /* @REPLACE_BRACKET: timer不存在 */
    if (NULL == timer)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ID */
        return (TTOS_INVALID_ID);
    }

    /* @KEEP_COMMENT: 禁止虚拟中断 */
    TBSP_GLOBALINT_DISABLE (msr);

    /* @REPLACE_BRACKET: timer状态是TTOS_TIMER_UNINSTALL */
    if (TTOS_TIMER_UNINSTALL == timer->state)
    {
        /* @KEEP_COMMENT: 使能虚拟中断 */
        TBSP_GLOBALINT_ENABLE (msr);

        /* @KEEP_COMMENT: 重新使能调度 */
        (void)ttosEnableTaskDispatchWithLock ();

        /* @REPLACE_BRACKET: TTOS_INVALID_STATE */
        return (TTOS_INVALID_STATE);
    }

    /* @REPLACE_BRACKET: timer状态是TTOS_TIMER_ACTIVE */
    if (TTOS_TIMER_ACTIVE == timer->state)
    {
        /*
         * @KEEP_COMMENT: 调用ttosExactWaitedTimer(DT.4.8)将timer从定时器等待
         * 队列中移出
         */
        (void)ttosExactWaitedTimer (timer);
    }

    /* @KEEP_COMMENT: 设置timer初始参数 */
    timer->waitedTicks = timer->ticks;
    timer->remainCount = timer->repeatCount;

    /* @KEEP_COMMENT: 调用ttosInsertWaitedTimer(DT.4.9)将timer插入定时器等待队列
     */
    (void)ttosInsertWaitedTimer (timer);

    /* @KEEP_COMMENT: 设置timer状态为TTOS_TIMER_ACTIVE */
    timer->state = (T_UWORD)TTOS_TIMER_ACTIVE;

    /* @KEEP_COMMENT: 使能虚拟中断 */
    TBSP_GLOBALINT_ENABLE (msr);

    /* @KEEP_COMMENT: 重新使能调度 */
    (void)ttosEnableTaskDispatchWithLock ();

    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}

/**
 * @brief:
 *    更新定时器记时参数
 * @param[in]: timerID: 定时器的ID
 * @return:
 *    TTOS_INVALID_ID：<timerID>指定的对象类型不是定时器；
 *                     <timerID>指定的定时器不存在。
 *    TTOS_OK：更新参数成功。
 * @implements: DT.4.2
 */
T_TTOS_ReturnCode TTOS_UpdateTimer (TIMER_ID timerID, T_UWORD ticks)
{
    TBSP_MSR_TYPE             msr = 0U;
    T_TTOS_TimerControlBlock *timer;

    /* @KEEP_COMMENT: 获取<timerID>指定的定时器记录为timer */
    timer = (T_TTOS_TimerControlBlock *)(ttosGetObjectById (timerID,
                                                            TTOS_OBJECT_TIMER));

    /* @REPLACE_BRACKET: timer不存在 */
    if (NULL == timer)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ID */
        return (TTOS_INVALID_ID);
    }

    /* @KEEP_COMMENT: 禁止虚拟中断 */
    TBSP_GLOBALINT_DISABLE (msr);

    /* @REPLACE_BRACKET: timer状态是TTOS_TIMER_ACTIVE */
    if (TTOS_TIMER_ACTIVE == timer->state)
    {
        /*
         * @KEEP_COMMENT: 调用ttosExactWaitedTimer(DT.4.8)将timer从定时器等待
         * 队列中移出
         */
        (void)ttosExactWaitedTimer (timer);
    }

    /* @KEEP_COMMENT: 设置timer初始参数 */
    timer->waitedTicks = ticks;

    /* @KEEP_COMMENT: 调用ttosInsertWaitedTimer(DT.4.9)将timer插入定时器等待队列
     */
    (void)ttosInsertWaitedTimer (timer);

    /* @KEEP_COMMENT: 设置timer状态为TTOS_TIMER_ACTIVE */
    timer->state = (T_UWORD)TTOS_TIMER_ACTIVE;

    /* @KEEP_COMMENT: 使能虚拟中断 */
    TBSP_GLOBALINT_ENABLE (msr);

    /* @KEEP_COMMENT: 重新使能调度 */
    (void)ttosEnableTaskDispatchWithLock ();

    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}
