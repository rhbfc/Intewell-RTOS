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
 *    实现周期任务截止时间等待队列的时间通知。
 * @return:
 *    无
 * @implements: RTE_DTASK.25.1
 */
void ttosPeriodExpireNotify (void)
{
    T_UWORD                  first, time_temp, lJobCompleted;
    TBSP_MSR_TYPE            msr = 0U;
    struct list_node        *node;
    T_TTOS_TaskControlBlock *task;
    /* @KEEP_COMMENT: 初始化变量first为TRUE */
    first               = TRUE;
    T_UWORD nonRunState = (T_UWORD)TTOS_TASK_NONRUNNING_STATE;
    T_UWORD pwaitState  = (T_UWORD)TTOS_TASK_PWAITING;
    T_UWORD readyState  = (T_UWORD)TTOS_TASK_READY;
    T_UWORD cwaitState  = (T_UWORD)TTOS_TASK_CWAITING;
    /* @KEEP_COMMENT: 禁止虚拟中断 */
    TBSP_GLOBALINT_DISABLE (msr);

    /* @REPLACE_BRACKET: 1 */
    while (1)
    {
        /*
         * @KEEP_COMMENT:
         * 调用list_first(DT.8.9)获取截止期时间等待队列
         * ttosManager.expireWaitedQueue中第一个任务存放至变量node
         */
        node = list_first(&(ttosManager.expireWaitedQueue));

        /* @REPLACE_BRACKET: node任务不存在 */
        if (NULL == node)
        {
            break;
        }

        /* @REPLACE_BRACKET: first等于FALSE && node任务的等待时间为零 */
        if ((FALSE == first)
            && (0 != ((T_TTOS_PeriodTaskNode *)node)->waitedTime))
        {
            /* @KEEP_COMMENT: 根据node任务的等待时间设置截止期等待队列的时间通知
             */
            (void)tbspSetPExpireTick (
                ((T_TTOS_PeriodTaskNode *)node)->waitedTime);
            break;
        }

        first = FALSE;
        /*
         * @KEEP_COMMENT:
         * 设置node任务的截止期等待时间为node任务的周期时间减去node任务的
         * 持续时间再减去node任务的补偿时间
         */
        ((T_TTOS_PeriodTaskNode *)node)->waitedTime
            = ((T_TTOS_PeriodTaskNode *)node)->periodTime
              - ((T_TTOS_PeriodTaskNode *)node)->durationTime
              - ((T_TTOS_PeriodTaskNode *)node)->replenishTime;
        /* @KEEP_COMMENT: 设置node任务的补偿时间为零 */
        ((T_TTOS_PeriodTaskNode *)node)->replenishTime = 0U;
        /* @KEEP_COMMENT:
         * 调用list_delete(DT.8.8)将node任务从截止期等待队列上移除 */
        list_delete(node);
        task = (T_TTOS_TaskControlBlock *)(((T_TTOS_PeriodTaskNode *)node)
                                               ->task);
        if (task == NULL)
        {
            return;
        }
        /* @KEEP_COMMENT: 将node任务的周期完成标记保存至变量lJobCompleted */
        lJobCompleted = task->periodNode.jobCompleted;
        /* @KEEP_COMMENT: 清除node任务的周期完成标记和TTOS_TASK_CWAITING状态 */
        task->periodNode.jobCompleted = FALSE;
        task->state &= (~cwaitState);

        /* @REPLACE_BRACKET: FALSE == lJobCompleted */
        if (FALSE == lJobCompleted)
        {
            /*由于此时周期任务不在截止期链表上，因此超时钩子函数中不能调用改变周期任务状态相关的接口*/
            /* @KEEP_COMMENT:
             * 根据node任务的ID号调用TTOS_ExpireTaskHook()进行周期超时处理 */
            (void)TTOS_ExpireTaskHook (task->taskControlId);
        }

        /* @REPLACE_BRACKET: node任务等待下一个周期到来的时间不等于零 */
        if (0 != ((T_TTOS_PeriodTaskNode *)node)->waitedTime)
        {
            /* @KEEP_COMMENT:
             * 调用tbspGetPWaitTick(DT.6.11)获取周期等待队列已等待的时间 */
            time_temp = tbspGetPWaitTick ();
            /*
             * @KEEP_COMMENT:
             * 调用ttosInsertWaitedPeriodTask(DT.2.25)将node任务插入周期等待
             * 队列ttosManager.periodWaitedQueue并获取通知时间存放至变量time
             */
            time_temp = ttosInsertWaitedPeriodTask (
                (T_TTOS_PeriodTaskNode *)node, &ttosManager.periodWaitedQueue,
                time_temp);

            /* @REPLACE_BRACKET: time不等于0 */
            if (0U != time_temp)
            {
                /* @KEEP_COMMENT: 根据time设置周期等待队列的通知时间 */
                (void)tbspSetPWaitTick (time_temp);
            }

            /* @REPLACE_BRACKET: node任务状态为TTOS_TASK_READY */
            if (TTOS_TASK_READY == (task->state & readyState))
            {
                /* @KEEP_COMMENT:
                 * 调用ttosClearTaskReady(DT.2.21)清除node任务的就绪状态 */
                (void)ttosClearTaskReady (task);
            }

            /* @KEEP_COMMENT:
             * 将node任务的状态设置为当前状态与TTOS_TASK_PWAITING的复合态 */
            task->state |= pwaitState;
        }

        else
        {
            /* @KEEP_COMMENT: 设置node任务的截止期等待时间为node任务的持续时间
             */
            ((T_TTOS_PeriodTaskNode *)node)->waitedTime
                = ((T_TTOS_PeriodTaskNode *)node)->durationTime;
            /*
             * @KEEP_COMMENT:
             * 调用ttosInsertWaitedPeriodTask(DT.2.25)将node任务插入截止期
             * 等待队列ttosManager.expireWaitedQueue
             */
            (void)ttosInsertWaitedPeriodTask ((T_TTOS_PeriodTaskNode *)node,
                                              &ttosManager.expireWaitedQueue,
                                              0U);

            /*
             * @REPLACE_BRACKET: lJobCompleted等于TRUE && node任务的状态为
             * TTOS_TASK_NONRUNNING_STATE
             */
            if ((TRUE == lJobCompleted) && (0 == (task->state & nonRunState)))
            {
                /* @KEEP_COMMENT:
                 * 调用ttosSetTaskReady(DT.2.32)设置node任务为就绪任务 */
                //(void)ttosSetTaskReady (task);
                ttosPeriodPrioReadyQueueAdd (task);
            }
        }
    }

    /* @KEEP_COMMENT: 调用tbspSetGlobalInt(DT.6.3)使能虚拟中断 */
    TBSP_GLOBALINT_ENABLE (msr);
}
