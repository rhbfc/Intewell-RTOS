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

/*
 * @brief:
 *    实现周期任务周期等待队列的时间通知。
 * @return:
 *    无
 * @implements: RTE_DTASK.27.1
 */
void ttosPeriodWaitNotify (void)
{
    T_UWORD                  first, time_temp;
    TBSP_MSR_TYPE            msr = 0U;
    struct list_node        *node;
    T_TTOS_TaskControlBlock *task;
    T_UWORD                  nonRunState = (T_UWORD)TTOS_TASK_NONRUNNING_STATE;
    T_UWORD                  pwaitState  = (T_UWORD)TTOS_TASK_PWAITING;
    /* @KEEP_COMMENT: 初始化变量first为TRUE */
    first = TRUE;
    /* @KEEP_COMMENT: 调用tbspClearGlobalInt(DT.6.4)禁止虚拟中断 */
    TBSP_GLOBALINT_DISABLE (msr);

    /* @REPLACE_BRACKET: 1 */
    while (1)
    {
        /*
         * @KEEP_COMMENT:
         * 调用list_first(DT.8.9)获取周期等待队列中第一个任务
         * 存放至变量node
         */
        node = list_first(&(ttosManager.periodWaitedQueue));

        /* @REPLACE_BRACKET: node任务不存在 */
        if (NULL == node)
        {
            break;
        }

        /* @REPLACE_BRACKET: first等于FALSE && node任务的等待时间为零 */
        if ((FALSE == first)
            && (((T_TTOS_PeriodTaskNode *)node)->waitedTime != 0U))
        {
            /* @KEEP_COMMENT: 根据node任务的等待时间设置周期等待队列的通知时间
             */
            (void)tbspSetPWaitTick (
                ((T_TTOS_PeriodTaskNode *)node)->waitedTime);
            break;
        }

        /* @KEEP_COMMENT: first = FALSE */
        first = FALSE;
        /* @KEEP_COMMENT: 设置node任务的周期等待时间为node任务的持续时间 */
        ((T_TTOS_PeriodTaskNode *)node)->waitedTime
            = ((T_TTOS_PeriodTaskNode *)node)->durationTime;
        /* @KEEP_COMMENT:
         * 调用list_delete(DT.8.8)将node任务从周期等待队列上移除 */
        list_delete(node);
        /*
         * @KEEP_COMMENT:
         * 调用tbspGetPExpireTick(DT.6.12)获取周期任务截止期等待队列已经
         * 等待的时间
         */
        time_temp = tbspGetPExpireTick ();
        /*
         * @KEEP_COMMENT:
         * 调用ttosInsertWaitedPeriodTask(DT.2.25)将node任务插入截止期等待
         * 队列ttosManager.expireWaitedQueue并获取队列被通知时间存放至变量time
         */
        time_temp = ttosInsertWaitedPeriodTask ((T_TTOS_PeriodTaskNode *)node,
                                                &ttosManager.expireWaitedQueue,
                                                time_temp);

        /* @REPLACE_BRACKET: time不等于0 */
        if (0U != time_temp)
        {
            /* @KEEP_COMMENT: 根据time设置周期截止期等待队列的通知时间 */
            (void)tbspSetPExpireTick (time_temp);
        }

        task = (T_TTOS_TaskControlBlock *)(((T_TTOS_PeriodTaskNode *)node)
                                               ->task);
        if (task == NULL)
        {
            return;
        }
        /* @KEEP_COMMENT: 清除node任务的TTOS_TASK_PWAITING状态 */
        task->state &= (~pwaitState);

        /* @REPLACE_BRACKET: node任务的状态不为TTOS_TASK_NONRUNNING_STATE */
        if (0 == (task->state & nonRunState))
        {
            /* @KEEP_COMMENT:
             * 调用ttosSetTaskReady(DT.2.32)设置node任务为就绪任务 */
            //(void)ttosSetTaskReady (task);
            ttosPeriodPrioReadyQueueAdd (task);
        }
    }

    /* @KEEP_COMMENT: 调用tbspSetGlobalInt(DT.6.5)使能虚拟中断 */
    TBSP_GLOBALINT_ENABLE (msr);
}
