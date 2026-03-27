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

/* 周期等待队列链上第一个周期任务的周期等待时间 */
T_EXTERN T_UWORD pwaitTicks;

/*截止期等待队列链上第一个周期任务的截止期等待时间 */
T_EXTERN T_UWORD pexpireTicks;

/* @MOD_EXTERN> */

/************************前向声明******************************/
/************************模块变量******************************/
/************************全局变量******************************/

/* @<MOD_VAR */

/*周期临时等待队列链表头*/
struct list_head periodWaitedQueueTmp;

/*周期截止时间临时等待队列链表头。*/
struct list_head expireWaitedQueueTmp;

/* @MOD_VAR> */

/************************实   现*******************************/
/* @MODULE> */

/*
 * @brief:
 *    根据周期等待通知到时的剩余ticks来更新周期任务周期等待的时间。
 * @param[in]: pwaiteLeftTicks: 周期等待通知到时剩余ticks
 * @return:
 *    无
 * @implements: RTE_DTASK.26.1
 */
void ttosPeriodWaitQueueModify (T_UWORD pwaiteLeftTicks)
{
    struct list_node        *node;
    struct list_node        *nextNode;
    T_TTOS_TaskControlBlock *task;
    T_TTOS_PeriodTaskNode   *pTaskNode;
    T_UWORD                  remainder   = 0U;
    TBSP_MSR_TYPE            msr         = 0U;
    T_UWORD                  nonRunState = (T_UWORD)TTOS_TASK_NONRUNNING_STATE;
    T_UWORD                  pwaitState  = (T_UWORD)TTOS_TASK_PWAITING;
    /* @KEEP_COMMENT: 禁止虚拟中断 */
    TBSP_GLOBALINT_DISABLE (msr);
    /*
     * @KEEP_COMMENT:
     * 调用list_first(DT.8.9)获取周期等待队列中第一个任务
     * 存放至变量node
     */
    node = list_first(&(ttosManager.periodWaitedQueue));

    /* @REPLACE_BRACKET: node任务不存在 */
    if (NULL == node)
    {
        /* @KEEP_COMMENT: 调用tbspSetGlobalInt(DT.6.3)使能虚拟中断 */
        TBSP_GLOBALINT_ENABLE (msr);
        return;
    }

    /*
     *周期等待队列链上的第一个周期任务的时间到了，应该设置为0，便于使用剩余时间
     *对周期等待队列链上的周期任务做统一的处理。临时周期等待队列上的周期任务
     *才能正确合并到周期等待队列上，否则时间会存在更快的效果。
     */
    /* @KEEP_COMMENT: 设置node任务的等待时间为0 */
    ((T_TTOS_PeriodTaskNode *)node)->waitedTime = 0U;

    /* @REPLACE_BRACKET: 1 */
    while (1)
    {
        /* @REPLACE_BRACKET: 0 == <pwaiteLeftTicks>  */
        if (0U == pwaiteLeftTicks)
        {
            break;
        }

        pTaskNode = (T_TTOS_PeriodTaskNode *)node;
        if (pTaskNode == NULL)
        {
            return;
        }

        /* @REPLACE_BRACKET: <pwaiteLeftTicks>小于node任务的周期等待时间 */
        if (pwaiteLeftTicks < pTaskNode->waitedTime)
        {
            /*完成剩余时间的处理*/
            /* @KEEP_COMMENT: node任务的等待时间-= <pwaiteLeftTicks>  */
            pTaskNode->waitedTime -= pwaiteLeftTicks;
            break;
        }

        else
        {
            /*继续处理剩余时间*/
            /* @KEEP_COMMENT: <pwaiteLeftTicks> -= node任务的等待时间 */
            pwaiteLeftTicks -= pTaskNode->waitedTime;
        }

        task = (T_TTOS_TaskControlBlock *)(pTaskNode->task);
        if (task == NULL)
        {
            return;
        }

        /* @REPLACE_BRACKET: <pwaiteLeftTicks>大于等于node任务截止期时间 */
        if (pwaiteLeftTicks >= pTaskNode->durationTime)
        {
            /* 只要剩余时间大于周期任务的截止期，则说明至少在一个截止期内没有执行，也就是周期没有完成*/
            /* @KEEP_COMMENT:
             * 根据node任务的ID号调用TTOS_ExpireTaskHook()进行周期超时处理 */
            (void)TTOS_ExpireTaskHook (task->taskControlId);
        }

        /* @KEEP_COMMENT: 计算<pwaiteLeftTicks> %node任务周期的余数remainder */
        remainder = pwaiteLeftTicks % pTaskNode->periodTime;
        /* @KEEP_COMMENT:
         * 获取ttosManager.periodWaitedQueue下一个周期任务nextNode */
        nextNode = list_next(node, &(ttosManager.periodWaitedQueue));

        /* @REPLACE_BRACKET: 0 != remainder */
        if (0U != remainder)
        {
            /* remainder为0时，说明周期等待时间刚好完成，由周期等待队列通知函数通知，否则被放在临时等待队列上*/
            /* @KEEP_COMMENT:
             * 调用list_delete(DT.8.8)将node任务从周期等待队列上移除 */
            list_delete(node);
        }

        /* 周期等待时间到时，根据剩余时间来判断此任务在绝对时间所处的队列 */
        /* @REPLACE_BRACKET: 0 == remainder */
        if (0U == remainder)
        {
            /* 周期等待时间刚好完成，由周期等待队列通知函数通知*/
            /* @KEEP_COMMENT: 设置node任务的等待时间为0 */
            pTaskNode->waitedTime = 0U;
        }

        /* @REPLACE_BRACKET:  remainder小于node任务的截止期时间 */
        else if (remainder < pTaskNode->durationTime)
        {
            /*此周期任务应该放在临时截止期等待队列链上，等待截止期的到来*/
            /* @KEEP_COMMENT: 设置node任务的等待时间为截止期时间- remainder */
            pTaskNode->waitedTime = pTaskNode->durationTime - remainder;
            /*
             * @KEEP_COMMENT:
             * 调用list_add_tail(DT.8.7)将node任务插入截止期
             * 临时等待队列expireWaitedQueueTmp
             */
            list_add_tail(node, &expireWaitedQueueTmp);
            /* @KEEP_COMMENT: 清除node任务的TTOS_TASK_PWAITING状态 */
            task->state &= (~pwaitState);

            /*截止期等待链上的周期任务的状态仅仅是周期等待态时，才需要被唤醒*/

            /* @REPLACE_BRACKET: node任务的状态不为TTOS_TASK_NONRUNNING_STATE */
            if (0 == (task->state & nonRunState))
            {
                                /* @KEEP_COMMENT:
                 * 调用ttosSetTaskReady(DT.2.32)设置node任务为就绪任务 */
                //(void)ttosSetTaskReady (task);
                ttosPeriodPrioReadyQueueAdd (task);
            }
        }

        else
        {
            /*此周期任务应该放在临时周期等待队列链上，等待周期的到来*/
            /* 余数与执行时间相等，并且截止时间与周期相等
             * 时，remainder为0，并且等待时间为0，在周期等待通知时会移到截止时间等待队列中*/
            /* @KEEP_COMMENT: 设置node任务的等待时间为node任务周期- remainder */
            pTaskNode->waitedTime = pTaskNode->periodTime - remainder;
            /*
             * @KEEP_COMMENT: 调用list_add_tail(DT.8.7)将node任务插入周期
             * 临时等待队列periodWaitedQueueTmp
             */
            list_add_tail(node, &periodWaitedQueueTmp);
        }

        /* @REPLACE_BRACKET: nextNode任务不存在 */
        if (NULL == nextNode)
        {
            break;
        }

        /* @KEEP_CODE:  */
        node = nextNode;
    }

    /* @KEEP_COMMENT: 调用tbspSetGlobalInt(DT.6.3)使能虚拟中断 */
    TBSP_GLOBALINT_ENABLE (msr);
}

/*
 * @brief:
 *    根据截止期等待通知到时的剩余ticks来更新周期任务截止期等待的时间。
 * @param[in]: pexpireLeftTicks: 截止期等待通知到时剩余ticks
 * @return:
 *    无
 * @implements: RTE_DTASK.26.2
 */
void ttosPeriodExpireQueueModify (T_UWORD pexpireLeftTicks)
{
    struct list_node        *node;
    struct list_node        *nextNode;
    T_TTOS_TaskControlBlock *task;
    T_TTOS_PeriodTaskNode   *pTaskNode;
    T_UWORD                  remainder = 0U;
    T_UWORD                  jobCompletedOld;
    TBSP_MSR_TYPE            msr         = 0U;
    /* @KEEP_COMMENT: 禁止虚拟中断 */
    TBSP_GLOBALINT_DISABLE (msr);
    /*
     * @KEEP_COMMENT:
     * 调用list_first(DT.8.9)获取截止期等待队列中第一个任务
     * 存放至变量node
     */
    node = list_first(&(ttosManager.expireWaitedQueue));

    /* @REPLACE_BRACKET: node任务不存在 */
    if (NULL == node)
    {
        /* @KEEP_COMMENT: 调用tbspSetGlobalInt(DT.6.3)使能虚拟中断 */
        TBSP_GLOBALINT_ENABLE (msr);
        return;
    }

    /*
     *截止期等待队列链上的第一个周期任务的时间到了，应该设置为0，便于使用剩余时间
     *对截止期等待队列链上的周期任务做统一的处理。临时截止期等待队列上的周期任务
     *才能正确合并到截止期等待队列上，否则时间会存在更快的效果。
     */
    /* @KEEP_COMMENT: 设置node任务的等待时间为0 */
    ((T_TTOS_PeriodTaskNode *)node)->waitedTime = 0U;

    /* @REPLACE_BRACKET: 1 */
    while (1)
    {
        /* @REPLACE_BRACKET: 0 == <pexpireLeftTicks> */
        if (0U == pexpireLeftTicks)
        {
            break;
        }

        pTaskNode = (T_TTOS_PeriodTaskNode *)node;
        if (pTaskNode == NULL)
        {
            return;
        }

        /* @REPLACE_BRACKET: <pexpireLeftTicks>小于node任务的周期等待时间 */
        if (pexpireLeftTicks < pTaskNode->waitedTime)
        {
            /*完成剩余时间的处理*/
            /* @KEEP_COMMENT: node任务的等待时间-= <pexpireLeftTicks> */
            pTaskNode->waitedTime -= pexpireLeftTicks;
            break;
        }

        else
        {
            /*继续处理剩余时间*/
            /* @KEEP_COMMENT: <pexpireLeftTicks> -= node任务的等待时间 */
            pexpireLeftTicks -= pTaskNode->waitedTime;
        }

        task = (T_TTOS_TaskControlBlock *)(pTaskNode->task);
        if (task == NULL)
        {
            return;
        }

        jobCompletedOld = pTaskNode->jobCompleted;

        /* @REPLACE_BRACKET: <pexpireLeftTicks>大于等于node任务周期时间 */
        if (pexpireLeftTicks >= pTaskNode->periodTime)
        {
            /* 只要剩余时间大于周期任务的周期，则说明至少在一个截止期内没有执行，也就是周期没有完成*/
            /* @KEEP_COMMENT: 设置node任务的周期完成标记为非完成状态 */
            pTaskNode->jobCompleted = FALSE;
        }

        /* @KEEP_COMMENT: 计算<pexpireLeftTicks> %node任务周期的余数remainder */
        remainder = pexpireLeftTicks % pTaskNode->periodTime;
        /*有可能会从截止期等待队列上摘除正在处理的周期任务，因此此处需要根据当前正在处理的周期任务获取下一个需要处理的周期任务*/
        /* @KEEP_COMMENT:
         * 获取ttosManager.expireWaitedQueue下一个周期任务nextNode */
        nextNode = list_next(node, &(ttosManager.expireWaitedQueue));

        /* @REPLACE_BRACKET: 0 != remainder */
        if (0U != remainder)
        {
            /* remainder为0时，说明截止时间刚好完成，由截止期等待队列通知函数通知，否则被放在临时等待队列上*/

            /* @REPLACE_BRACKET: 任务未完成 */
            if (FALSE == task->periodNode.jobCompleted)
            {
                /* @KEEP_COMMENT:
                 * 根据node任务的ID号调用TTOS_ExpireTaskHook()进行周期超时处理
                 */
                (void)TTOS_ExpireTaskHook (task->taskControlId);
            }

            /* @KEEP_COMMENT:
             * 调用list_delete(DT.8.8)将node任务从截止期等待队列上移除
             */
            list_delete(node);
        }

        /* 截止时间到时，根据剩余时间来判断此任务在绝对时间所处的队列*/
        /* @REPLACE_BRACKET: 0 == remainder */
        if (0U == remainder)
        {
            /* 截止时间刚好完成，由截止期等待队列通知函数通知周期任务的超时情况*/
            /* @KEEP_COMMENT: 设置node任务的等待时间为0 */
            pTaskNode->waitedTime = 0U;
            task->state &= (~TTOS_TASK_CWAITING);
            if ((TRUE == jobCompletedOld) && (0 == (task->state & TTOS_TASK_NONRUNNING_STATE)))
            {
                /* @KEEP_COMMENT: 调用ttosSetTaskReady(DT.2.32)设置node任务为就绪任务 */
                //ttosSetTaskReady(task);
                ttosPeriodPrioReadyQueueAdd (task);
            }
        }

        /* @REPLACE_BRACKET:  remainder小于node任务的周期等待时间 */
        else if (remainder < (pTaskNode->periodTime - pTaskNode->durationTime))
        {
            /*此周期任务应该放在临时周期等待队列链上，等待周期的到来*/
            /* @KEEP_COMMENT: 设置node任务的等待时间为周期等待时间- remainder */
            pTaskNode->waitedTime
                = pTaskNode->periodTime - pTaskNode->durationTime - remainder;
            /*
             * @KEEP_COMMENT: 调用list_add_tail(DT.8.7)将node任务插入周期
             * 临时等待队列periodWaitedQueueTmp
             */
            list_add_tail(node, &periodWaitedQueueTmp);
            /* @KEEP_COMMENT: 清除node任务的周期完成标记和TTOS_TASK_CWAITING状态
             */
            task->periodNode.jobCompleted = FALSE;
            task->state &= (~TTOS_TASK_CWAITING);

            /* @REPLACE_BRACKET: node任务状态为TTOS_TASK_READY */
            if (TTOS_TASK_READY == (task->state & TTOS_TASK_READY))
            {
                /* @KEEP_COMMENT:
                 * 调用ttosClearTaskReady(DT.2.21)清除node任务的就绪状态 */
                (void)ttosClearTaskReady (task);
            }

            /* @KEEP_COMMENT:
             * 将node任务的状态设置为当前状态与TTOS_TASK_PWAITING的复合态 */
            task->state |= TTOS_TASK_PWAITING;
        }

        else
        {
            /*此周期任务应该放在临时截止期等待队列链上，等待截止期的到来*/

            /* @KEEP_COMMENT: 清除node任务的周期完成标记和TTOS_TASK_CWAITING状态
             */
            task->periodNode.jobCompleted = FALSE;
            task->state &= (~TTOS_TASK_CWAITING);
            /* @KEEP_COMMENT: 设置node任务的等待时间为node任务周期- remainder */
            pTaskNode->waitedTime = pTaskNode->periodTime - remainder;
            /*
             * @KEEP_COMMENT:
             * 调用list_add_tail(DT.8.7)将node任务插入截止期
             * 临时等待队列expireWaitedQueueTmp
             */
            list_add_tail(node, &expireWaitedQueueTmp);

            /*截止期等待链上的周期任务的状态仅仅是完成等待态时，才需要被唤醒*/

            /*
             * @REPLACE_BRACKET: lJobCompleted等于TRUE && node任务的状态为
             * TTOS_TASK_NONRUNNING_STATE
             */
            if ((TRUE == jobCompletedOld) && (0 == (task->state & TTOS_TASK_NONRUNNING_STATE)))
            {
                /* @KEEP_COMMENT:
                 * 调用ttosSetTaskReady(DT.2.32)设置node任务为就绪任务 */
                //(void)ttosSetTaskReady (task);
                ttosPeriodPrioReadyQueueAdd (task);
            }
        }

        /* @REPLACE_BRACKET: nextNode任务不存在 */
        if (NULL == nextNode)
        {
            break;
        }

        /* @KEEP_CODE:  */
        node = nextNode;
    }

    /* @KEEP_COMMENT: 调用tbspSetGlobalInt(DT.6.3)使能虚拟中断 */
    TBSP_GLOBALINT_ENABLE (msr);
}

/*
 * @brief:
 *    合并临时队列的周期任务到周期或者截止时间等待队列上。
 * @return:
 *    TTOS_PERIOD_NONE_QUEUE_NOTIFY: 不通知周期任务周期和截止时间等待队列。
 *    TTOS_PERIOD_WAIT_QUEUE_NOTIFY: 通知周期任务周期等待队列。
 *    TTOS_PERIOD_EXPIRE_QUEUE_NOTIFY: 通知周期任务周期截止时间等待队列。
 *    TTOS_PERIOD_ALL_QUEUE_NOTIFY: 通知周期任务周期和截止时间等待队列。
 * @implements: RTE_DTASK.26.3
 */
T_TTOS_PeriodQueueNotifyType ttosPeriodQueueMerge (void)
{
    struct list_node            *node;
    T_UWORD                      time_temp;
    TBSP_MSR_TYPE                msr = 0U;
    T_TTOS_PeriodQueueNotifyType pQueueNotify;
    T_TTOS_PeriodQueueNotifyType expireQueueNotify
        = TTOS_PERIOD_EXPIRE_QUEUE_NOTIFY;
    /* @KEEP_COMMENT: 禁止虚拟中断 */
    TBSP_GLOBALINT_DISABLE (msr);
    /* @KEEP_COMMENT:
     * 设置周期队列通知类型变量pQueueNotify为TTOS_PERIOD_NONE_QUEUE_NOTIFY */
    pQueueNotify = TTOS_PERIOD_NONE_QUEUE_NOTIFY;
    T_UWORD uQueueNotify;

    /* @REPLACE_BRACKET: 1 */
    while (1)
    {
        /*
         * @KEEP_COMMENT:
         * 调用list_first(DT.8.9)获取周期临时等待队列中第一个任务
         * 存放至变量node
         */
        node = list_first(&periodWaitedQueueTmp);

        /* @REPLACE_BRACKET: node任务不存在 */
        if (NULL == node)
        {
            break;
        }

        /* @KEEP_COMMENT:
         * 调用list_delete(DT.8.8)将node任务从周期临时等待队列上移除 */
        list_delete(node);
        /* @KEEP_COMMENT:
         * 调用tbspGetPWaitTick(DT.6.11)获取周期等待队列已等待的时间 */
        time_temp = tbspGetPWaitTick ();
        /*
         * @KEEP_COMMENT:
         * 调用ttosInsertWaitedPeriodTask(DT.2.25)将node任务插入周期等待
         * 队列ttosManager.periodWaitedQueue并获取通知时间存放至变量time
         */
        time_temp = ttosInsertWaitedPeriodTask ((T_TTOS_PeriodTaskNode *)node,
                                                &ttosManager.periodWaitedQueue,
                                                time_temp);

        /* @REPLACE_BRACKET: time不等于0 */
        if (0U != time_temp)
        {
            /* @KEEP_COMMENT: 根据time设置周期等待队列的通知时间 */
            (void)tbspSetPWaitTick (time_temp);
        }
    }

    /* @REPLACE_BRACKET: 1 */
    while (1)
    {
        /*
         * @KEEP_COMMENT:
         * 调用list_first(DT.8.9)获取截止期临时等待队列中第一个任务
         * 存放至变量node
         */
        node = list_first(&expireWaitedQueueTmp);

        /* @REPLACE_BRACKET: node任务不存在 */
        if (NULL == node)
        {
            break;
        }

        /* @KEEP_COMMENT:
         * 调用list_delete(DT.8.8)将node任务从截止期临时等待队列上移除
         */
        list_delete(node);
        /*
         * @KEEP_COMMENT:
         * 调用tbspGetPExpireTick(DT.6.12)获取周期截止期等待队列已经 等待的时间
         */
        time_temp = tbspGetPExpireTick ();
        /*
         * @KEEP_COMMENT:
         * 调用ttosInsertWaitedPeriodTask(DT.2.25)将node任务插入周期等待
         * 队列ttosManager.periodWaitedQueue并获取队列被通知时间存放至变量time
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
    }

    /*
     * @KEEP_COMMENT:
     * 调用list_first(DT.8.9)获取周期等待队列中第一个任务
     * 存放至变量node
     */
    node = list_first(&(ttosManager.periodWaitedQueue));

    /* @REPLACE_BRACKET: NULL != node */
    if ((NULL != node))
    {
        /* @REPLACE_BRACKET:node任务的周期等待时间为0  */
        if (0 == ((T_TTOS_PeriodTaskNode *)node)->waitedTime)
        {
            /* @KEEP_CODE: */
            pQueueNotify = TTOS_PERIOD_WAIT_QUEUE_NOTIFY;
        }

        else
        {
            /* @REPLACE_BRACKET: 0 == pwaitTicks */
            if (0U == pwaitTicks)
            {
                /* @KEEP_COMMENT:
                 * 根据node任务的等待时间设置周期等待队列的时间通知 */
                (void)tbspSetPWaitTick (
                    ((T_TTOS_PeriodTaskNode *)node)->waitedTime);
            }
        }
    }

    /*
     * @KEEP_COMMENT:
     * 调用list_first(DT.8.9)获取截止期等待队列中第一个任务
     * 存放至变量node
     */
    node = list_first(&(ttosManager.expireWaitedQueue));

    /* @REPLACE_BRACKET: NULL != node */
    if (NULL != node)
    {
        /* @REPLACE_BRACKET: node任务的截止期等待时间为0 */
        if (0 == ((T_TTOS_PeriodTaskNode *)node)->waitedTime)
        {
            /* @KEEP_CODE: */
            uQueueNotify
                = (T_UWORD)((T_UWORD)pQueueNotify | (T_UWORD)expireQueueNotify);
            pQueueNotify = (T_TTOS_PeriodQueueNotifyType)uQueueNotify;
        }

        else
        {
            /* @REPLACE_BRACKET: 0 == pexpireTicks */
            if (0U == pexpireTicks)
            {
                /* @KEEP_COMMENT:
                 * 根据node任务的等待时间设置截止期等待队列的时间通知 */
                (void)tbspSetPExpireTick (
                    ((T_TTOS_PeriodTaskNode *)node)->waitedTime);
            }
        }
    }

    /* @KEEP_COMMENT: 调用tbspSetGlobalInt(DT.6.3)使能虚拟中断 */
    TBSP_GLOBALINT_ENABLE (msr);
    /* @REPLACE_BRACKET: pQueueNotify  */
    return (pQueueNotify);
}
