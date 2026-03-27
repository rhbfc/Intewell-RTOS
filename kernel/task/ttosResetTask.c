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

/**
 * @brief:
 *    复位指定的任务。
 * @param[in]: taskID: 任务的ID，当为TTOS_SELF_OBJECT_ID时表示重启当前任务
 * @return:
 *    TTOS_CALLED_FROM_ISR：在中断处理程序中执行此接口。
 *    TTOS_INVALID_ID：<taskID>所表示的对象类型不是任务；
 *                     不存在<taskID>指定的任务。
 *    TTOS_INVALID_STATE：<taskID>指定的任务处于休眠态。
 *     TTOS_RESOURCE_IN_USE:
 * <taskID>指定任务拥有互斥信号量或者任务是禁止删除的。
 *    TTOS_OK：复位指定的任务成功。
 * @implements: RTE_DTASK.29.1
 */
T_TTOS_ReturnCode TTOS_ResetTask (TASK_ID taskID)
{
    TBSP_MSR_TYPE            msr = 0U;
    T_TTOS_TaskControlBlock *task;
    T_UWORD                  pwaitState    = (T_UWORD)TTOS_TASK_PWAITING;
    T_UWORD                  firstRunState = (T_UWORD)TTOS_TASK_FIRSTRUN;
    T_UWORD                  readyState    = (T_UWORD)TTOS_TASK_READY;
    T_UWORD                  waitState     = (T_UWORD)TTOS_TASK_WAITING;
    T_UWORD                  state         = pwaitState | firstRunState;

    /* @REPLACE_BRACKET: 在虚拟中断处理程序中执行此接口 */
    if (TRUE == ttosIsISR ())
    {
        /* @REPLACE_BRACKET: TTOS_CALLED_FROM_ISR */
        return (TTOS_CALLED_FROM_ISR);
    }

    /* @REPLACE_BRACKET: <taskID>为TTOS_SELF_OBJECT_ID */
    if (TTOS_SELF_OBJECT_ID == taskID)
    {
        /* @KEEP_COMMENT:
         * 获取正在运行的任务ttosManager.runningTask的ID号存放至<taskID> */
        taskID = ttosGetRunningTask ()->taskControlId;
    }
    if (NULL == taskID)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ID */
        return (TTOS_INVALID_ID);
    }

    /* @KEEP_COMMENT: 获取<taskID>指定的任务存放至变量task */
    task = (T_TTOS_TaskControlBlock *)ttosGetObjectById (taskID,
                                                         TTOS_OBJECT_TASK);

    /* @REPLACE_BRACKET: task任务不存在 */
    if ((0ULL) == task)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ID */
        return (TTOS_INVALID_ID);
    }

    /* @KEEP_COMMENT: 检查任务是否拥有互斥量或者是否是禁止删除的*/
    /* @REPLACE_BRACKET: (task->resourceCount > U(0)) || (TRUE ==
     * task->disableDeleteFlag) */
    if ((task->resourceCount > 0U) || (TRUE == task->disableDeleteFlag))
    {
        /* 拥有互斥量的任务不能够被重启，因此恢复调度并返回资源在使用 */
        (void)ttosEnableTaskDispatchWithLock ();
        return (TTOS_RESOURCE_IN_USE);
    }

    /* @KEEP_COMMENT: 禁止虚拟中断 */
    TBSP_GLOBALINT_DISABLE (msr);

    /* @REPLACE_BRACKET: task任务状态是TTOS_TASK_DORMANT */
    if (TTOS_TASK_DORMANT == task->state)
    {
        /* @KEEP_COMMENT: 使能虚拟中断 */
        TBSP_GLOBALINT_ENABLE (msr);
        (void)ttosEnableTaskDispatchWithLock ();
        /* @REPLACE_BRACKET: TTOS_INVALID_STATE */
        return (TTOS_INVALID_STATE);
    }

    /* @REPLACE_BRACKET: task任务状态为TTOS_TASK_WAITING */
    if (0U != (task->state & waitState))
    {
        /* @REPLACE_BRACKET: task任务在计时等待 */
        if (task->objCore.objectNode.next != NULL)
        {
            /*
             * @KEEP_COMMENT:
             * 调用ttosExactWaitedTask(DT.2.24)将task任务从任务tick等待
             * 队列中移除
             */
            (void)ttosExactWaitedTask (task);
        }

        (void)ttosExtractTaskq (task->wait.queue, task);
    }

    /* @KEEP_COMMENT: 重置task任务的初始参数 */
    (void)ttosInitTaskParam (task);

    /* @REPLACE_BRACKET: task任务状态为TTOS_TASK_READY */
    if (readyState == (task->state & readyState))
    {
        /* @KEEP_COMMENT: 调用ttosClearTaskReady(DT.2.21)清除task任务的就绪状态
         */
        (void)ttosClearTaskReady (task);
    }

    /* @REPLACE_BRACKET: task任务是周期任务 */
    if (TTOS_SCHED_PERIOD == task->taskType)
    {
        T_UWORD time_temp;
        /* @KEEP_COMMENT: 设置task任务的补偿时间为零并且完成标记为FALSE */
        task->periodNode.replenishTime = 0U;
        task->periodNode.jobCompleted  = FALSE;

        /* @REPLACE_BRACKET: task任务状态为TTOS_TASK_PWAITING */
        if (pwaitState == (task->state & pwaitState))
        {
            /* @KEEP_COMMENT:
             * 调用tbspGetPWaitTick(DT.6.11)获取周期等待队列已经等待的时间 */
            time_temp = tbspGetPWaitTick ();
            /*
             * @KEEP_COMMENT:
             * 调用ttosExactWaitedPeriodTask(DT.2.23)将task任务从周期任
             * 务周期等待队列移除并保存移除后周期等待队列的被通知时间至变量time
             */
            time_temp = ttosExactWaitedPeriodTask (
                &(task->periodNode), &ttosManager.periodWaitedQueue, time_temp);

            /* @REPLACE_BRACKET: time不等于零 */
            if (0U != time_temp)
            {
                /* @KEEP_COMMENT: 根据<time>设置周期等待队列的通知时间 */
                (void)tbspSetPWaitTick (time_temp);
            }
        }

        else
        {
            /*
             * @KEEP_COMMENT:
             * 调用tbspGetPExpireTick(DT.6.12)获取周期截止期等待队列已
             * 经等待的时间存放至变量elapseTime
             */
            time_temp = tbspGetPExpireTick ();
            /*
             * @KEEP_COMMENT:
             * 调用ttosExactWaitedPeriodTask(DT.2.23)将task任务从周期截
             * 止期等待队列移除并保存移除后周期截止期等待队列的被通知时间至
             * 变量time
             */
            time_temp = ttosExactWaitedPeriodTask (
                &(task->periodNode), &ttosManager.expireWaitedQueue, time_temp);

            /* @REPLACE_BRACKET: time不等于零 */
            if (0U != time_temp)
            {
                /* @KEEP_COMMENT: 根据time设置周期截止期等待队列的通知时间 */
                (void)tbspSetPExpireTick (time_temp);
            }
        }

        /* @REPLACE_BRACKET: task任务延迟启动时间不为零 */
        if (task->periodNode.delayTime != 0U)
        {
            /* @KEEP_COMMENT: 设置task任务的周期等待时间为task任务的延迟启动时间
             */
            task->periodNode.waitedTime = task->periodNode.delayTime;
            /* @KEEP_COMMENT:
             * 调用tbspGetPWaitTick(DT.6.11)获取周期等待队列已经等待的时间 */
            time_temp = tbspGetPWaitTick ();
            /*
             * @KEEP_COMMENT:
             * 调用ttosInsertWaitedPeriodTask(DT.2.25)将task任务插入周
             * 期等待队列ttosManager.periodWaitedQueue并获取队列被通知时间存放
             * 至变量time
             */
            time_temp = ttosInsertWaitedPeriodTask (
                &(task->periodNode), &ttosManager.periodWaitedQueue, time_temp);

            /* @REPLACE_BRACKET: time不等于0 */
            if (0U != time_temp)
            {
                /* @KEEP_COMMENT: 根据time设置周期等待队列的通知时间 */
                (void)tbspSetPWaitTick (time_temp);
            }

            /* @KEEP_COMMENT: 设置task任务的状态为(TTOS_TASK_PWAITING |
             * TTOS_TASK_FIRSTRUN) */
            task->state = state;
        }

        else
        {
            /* @KEEP_COMMENT:
             * 设置task任务的周期截止期等待时间为task任务的持续时间 */
            task->periodNode.waitedTime = task->periodNode.durationTime;
            /* @KEEP_COMMENT:
             * 调用tbspGetPExpireTick(DT.6.12)获取截止期等待队列已等待的时间 */
            time_temp = tbspGetPExpireTick ();
            /*
             * @KEEP_COMMENT:
             * 调用ttosInsertWaitedPeriodTask(DT.2.25)将任务插入截止
             * 期等待队列ttosManager.expireWaitedQueue并获取队列被通知时间
             * 存放至变量time
             */
            time_temp = ttosInsertWaitedPeriodTask (
                &(task->periodNode), &ttosManager.expireWaitedQueue, time_temp);

            /* @REPLACE_BRACKET: time不等于0 */
            if (0U != time_temp)
            {
                /* @KEEP_COMMENT: 根据time设置截止期等待队列的通知时间 */
                (void)tbspSetPExpireTick (time_temp);
            }

            /* @KEEP_COMMENT: 设置task任务的状态为TTOS_TASK_FIRSTRUN */
            task->state = (T_UWORD)TTOS_TASK_FIRSTRUN;
            /* @KEEP_COMMENT:
             * 调用ttosSetTaskReady(DT.2.32)设置task任务为就绪任务 */
            (void)ttosSetTaskReady (task);
        }
    }

    else
    {
        /* @KEEP_COMMENT: 设置task任务的状态为TTOS_TASK_FIRSTRUN */
        task->state = (T_UWORD)TTOS_TASK_FIRSTRUN;
        /* @KEEP_COMMENT: 调用ttosSetTaskReady(DT.2.32)设置task任务为就绪任务 */
        (void)ttosSetTaskReady (task);
    }

    /* @KEEP_COMMENT: 使能虚拟中断 */
    TBSP_GLOBALINT_ENABLE (msr);
    (void)ttosEnableTaskDispatchWithLock ();
    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}
