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
 *    获取指定定时器的信息。
 * @param[in]: timerID: 定时器的ID
 * @param[out]: timerInfo: 定时器信息
 * @return:
 *    TTOS_INVALID_ID：<timerID>指定的对象类型不是定时器；
 *                     <timerID>指定的定时器不存在。
 *    TTOS_INVALID_ADDRESS：<timerInfo>为NULL。
 *    TTOS_OK：成功获取定时器的信息。
 * @implements: DT.4.3
 */
T_TTOS_ReturnCode TTOS_GetTimerInfo (TIMER_ID          timerID,
                                     T_TTOS_TimerInfo *timerInfo)
{
    T_TTOS_TimerControlBlock *timer;

    /* @REPLACE_BRACKET: <timerInfo>为NULL */
    if (NULL == timerInfo)
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

    /* @KEEP_COMMENT: 获取timer信息，保存至<timerInfo> */
    timerInfo->ticks       = timer->ticks;
    timerInfo->handler     = timer->handler;
    timerInfo->argument    = timer->argument;
    timerInfo->repeatCount = timer->repeatCount;
    (void)ttosEnableTaskDispatchWithLock ();
    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}

/**
 * @brief:
 *    获取可以创建定时器的最大数量。
 * @return:
 *    T_UWORD：系统允许创建的最大对象个数: 任务、定时器、信号量、消息队列。
 * @implements: RTE_DTIMER.11
 */
T_UWORD getTimerMaxNum (void)
{
    /* @REPLACE_BRACKET: ttosManager.objMaxNumber[TTOS_OBJECT_TIMER] */
    return (ttosManager.objMaxNumber[TTOS_OBJECT_TIMER]);
}
