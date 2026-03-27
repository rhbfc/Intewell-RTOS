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
 *    安装指定的定时器，设置相应参数。
 * @param[in]: timerID: 定时器的ID
 * @param[in]: ticks: 定时tick数
 * @param[in]: handler: 定时器处理程序
 * @param[in]: argument: 定时器处理程序参数
 * @param[in]: repeatCount: 触发次数。0表示单次触发，
 *                          TTOS_TIMER_LOOP_FOREVER表示永久触发
 * @return:
 *    TTOS_INVALID_ID：<timerID>指定的对象类型不是定时器；
 *                     <timerID>指定的定时器不存在。
 *    TTOS_INVALID_TIME：<ticks>为0。
 *    TTOS_INVALID_ADDRESS：<handler>为NULL。
 *    TTOS_OK：安装成功。
 * @implements: DT.4.6
 */
T_TTOS_ReturnCode TTOS_InstallTimer (TIMER_ID timerID, T_UWORD ticks,
                                     T_TTOS_TimerRoutine handler,
                                     T_VOID *argument, T_UWORD repeatCount)
{
    TBSP_MSR_TYPE             msr = 0U;
    T_TTOS_TimerControlBlock *timer;

    /* @REPLACE_BRACKET: <ticks>为0 */
    if (0U == ticks)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_TIME */
        return (TTOS_INVALID_TIME);
    }

    /* @KEEP_COMMENT: 获取<timerID>指定的定时器，记录为timer */
    timer = (T_TTOS_TimerControlBlock *)ttosGetObjectById (timerID,
                                                           TTOS_OBJECT_TIMER);

    /* @REPLACE_BRACKET: timer不存在 */
    if (NULL == timer)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ID */
        return (TTOS_INVALID_ID);
    }

    /* @REPLACE_BRACKET: <handler>为空 */
    if (NULL == handler)
    {
        /* @KEEP_COMMENT: 重新使能调度 */
        (void)ttosEnableTaskDispatchWithLock ();

        /* @REPLACE_BRACKET: TTOS_INVALID_ADDRESS */
        return (TTOS_INVALID_ADDRESS);
    }

    /* 关闭中断进行定时器的访问互斥 */
    /* @KEEP_COMMENT: 禁止虚拟中断 */
    TBSP_GLOBALINT_DISABLE (msr);

    /* @KEEP_COMMENT: 设置timer参数 */
    timer->ticks       = ticks;
    timer->handler     = handler;
    timer->argument    = argument;
    timer->repeatCount = repeatCount;

    /* @REPLACE_BRACKET: timer状态为TTOS_TIMER_UNINSTALL */
    if (TTOS_TIMER_UNINSTALL == timer->state)
    {
        /* @KEEP_COMMENT: 设置timer状态为TTOS_TIMER_INSTALLED */
        timer->state = (T_UWORD)TTOS_TIMER_INSTALLED;
    }

    /* @KEEP_COMMENT: 使能虚拟中断 */
    TBSP_GLOBALINT_ENABLE (msr);

    /* @KEEP_COMMENT: 重新使能调度 */
    (void)ttosEnableTaskDispatchWithLock ();

    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}
