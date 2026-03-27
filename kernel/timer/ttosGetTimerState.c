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
/************************实    现******************************/

/* @MODULE> */

/**
 * @brief:
 *    获取指定定时器的状态。
 * @param[in]: timerID: 定时器的ID
 * @param[out]: state: 定时器状态
 * @return:
 *    TTOS_INVALID_ID：<timerID>指定的对象类型不是定时器；
 *                     <timerID>指定的定时器不存在。
 *    TTOS_INVALID_ADDRESS：<state>为NULL。
 *    TTOS_OK：成功获取定时器的状态。
 * @implements: DT.4.5
 */
T_TTOS_ReturnCode TTOS_GetTimerState (TIMER_ID           timerID,
                                      T_TTOS_TimerState *state)
{
    T_TTOS_TimerControlBlock *timer;

    /* @REPLACE_BRACKET: <state>为NULL */
    if (NULL == state)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ADDRESS */
        return (TTOS_INVALID_ADDRESS);
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

    /* @KEEP_COMMENT: 获取timer状态信息，保存至<state> */
    *state = timer->state;

    /* @KEEP_COMMENT: 重新使能调度 */
    (void)ttosEnableTaskDispatchWithLock ();

    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}
