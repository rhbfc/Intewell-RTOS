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

/************************头 文 件******************************/
/* @<MOD_HEAD */
#include <system/macros.h>
#include <ttosBase.h>
#include <ttosInterHal.h>
#include <ttosUtils.inl>

/* @MOD_HEAD> */

/* @<MOD_EXTERN */
T_EXTERN T_UWORD pwaitCurentTicks, pexpireCurrentTicks;
/* @MOD_EXTERN> */
/* @MODULE> */

/************************前向声明******************************/
/*
 * @brief
 *       从任务等待链表、定时器等待链表、周期等待时间和截止期等待时间找到等待最少的ticks
 * @param[out]   waitTicks: 内核相关信息
 * @return
 *    TTOS_INVALID_ADDRESS：传入参数地址为空
 *    TTOS_OK:获取信息成功
 * @implements: RTE_DUTILS.5.1
 */
T_TTOS_ReturnCode TTOS_GetWaitLeastTicks (T_UWORD *waitTicks)
{
    TBSP_MSR_TYPE     msr        = 0U;
    struct list_node *taskNode   = NULL;
    struct list_node *timerNode  = NULL;
    T_UWORD           taskTicks  = 0U;
    T_UWORD           timerTicks = 0U;
    T_UWORD           tmpPwaitCurentTicks;
    T_UWORD           tmpPexpireCurrentTicks;

    /* @REPLACE_BRACKET: NULL == waitTicks */
    if (NULL == waitTicks)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ADDRESS */
        return (TTOS_INVALID_ADDRESS);
    }

    /* @KEEP_COMMENT: 禁止虚拟中断 */
    TBSP_GLOBALINT_DISABLE_WITH_LOCK (msr);
    /* @KEEP_COMMENT: 任务的等待链表 */
    taskNode = list_first(&(ttosManager.taskWaitedQueue));
    /* @KEEP_COMMENT: 定时器等待链表 */
    timerNode  = list_first(&(ttosManager.timerWaitedQueue));
    taskTicks  = (taskNode == NULL)
                     ? T_UWORD_MAX
                     : ((T_TTOS_TaskControlBlock *)taskNode)->waitedTicks;
    timerTicks = (timerNode == NULL)
                     ? T_UWORD_MAX
                     : ((T_TTOS_TimerControlBlock *)timerNode)->waitedTicks;
    tmpPwaitCurentTicks
        = (pwaitCurentTicks == 0U) ? T_UWORD_MAX : pwaitCurentTicks;
    tmpPexpireCurrentTicks
        = (pexpireCurrentTicks == 0U) ? T_UWORD_MAX : pexpireCurrentTicks;
    *waitTicks = min (tmpPexpireCurrentTicks,
                      min (tmpPwaitCurentTicks, min (taskTicks, timerTicks)));
    /* @KEEP_COMMENT: 使能虚拟中断 */
    TBSP_GLOBALINT_ENABLE_WITH_LOCK (msr);
    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}
