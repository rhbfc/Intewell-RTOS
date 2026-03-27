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
 *    停止任务。
 * @param[out]||[in]: taskCB: 任务控制块
 * @return:
 *    无
 * @implements: RTE_DTASK.37.1
 */
void ttosStopTask (T_TTOS_TaskControlBlock *taskCB)
{
    T_UWORD waitState  = (T_UWORD)TTOS_TASK_WAITING;
    T_UWORD pwaitState = (T_UWORD)TTOS_TASK_PWAITING;
    T_UWORD readyState = (T_UWORD)TTOS_TASK_READY;
    /* @REPLACE_BRACKET: taskCB任务状态为TTOS_TASK_WAITING */
    if (0U != (taskCB->state & waitState))
    {
        /* @REPLACE_BRACKET: taskCB任务在计时等待 */
        if (taskCB->objCore.objectNode.next != NULL)
        {
            /*
             * @KEEP_COMMENT:
             * 调用ttosExactWaitedTask(DT.2.24)将taskCB任务从任务tick等待
             * 队列中移除
             */
            (void)ttosExactWaitedTask (taskCB);
        }

        (void)ttosExtractTaskq (taskCB->wait.queue, taskCB);
    }

    /* @KEEP_COMMENT: 重置taskCB任务的初始参数 */
    (void)ttosInitTaskParam (taskCB);

    /* @REPLACE_BRACKET: taskCB任务是周期任务 */
    if (TTOS_SCHED_PERIOD == taskCB->taskType)
    {
        T_UWORD time_temp;

        /* @REPLACE_BRACKET: taskCB任务的状态是TTOS_TASK_PWAITING */
        if (TTOS_TASK_PWAITING == (taskCB->state & pwaitState))
        {
            /* @KEEP_COMMENT:
             * 调用tbspGetPWaitTick(DT.6.11)获取周期等待队列已经等待的时间 */
            time_temp = tbspGetPWaitTick ();
            /*
             * @KEEP_COMMENT:
             * 调用ttosExactWaitedPeriodTask(DT.2.23)将taskCB任务从周期任务
             * 周期等待队列移除并保存移除后周期等待队列的被通知时间至变量time
             */
            time_temp = ttosExactWaitedPeriodTask (
                &(taskCB->periodNode), &ttosManager.periodWaitedQueue,
                time_temp);

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
             * 调用tbspGetPExpireTick(DT.6.12)获取截止期等待队列已 等待的时间
             */
            time_temp = tbspGetPExpireTick ();
            /*
             * @KEEP_COMMENT:
             * 调用ttosExactWaitedPeriodTask(DT.2.23)将taskCB任务从截止期
             * 等待队列移除并保存移除后周期截止期等待队列的被通知时间至变量time
             */
            time_temp = ttosExactWaitedPeriodTask (
                &(taskCB->periodNode), &ttosManager.expireWaitedQueue,
                time_temp);

            /* @REPLACE_BRACKET: time不等于零 */
            if (0U != time_temp)
            {
                /* @KEEP_COMMENT: 根据time设置截止期等待队列的通知时间 */
                (void)tbspSetPExpireTick (time_temp);
            }
        }

        /* @KEEP_COMMENT: 设置taskCB任务的补偿时间为0 */
        taskCB->periodNode.replenishTime = 0U;
        /* @KEEP_COMMENT: 设置taskCB任务的工作完成标记为FALSE */
        taskCB->periodNode.jobCompleted = FALSE;
    }

    /* @REPLACE_BRACKET: taskCB任务状态为TTOS_TASK_READY */
    if (TTOS_TASK_READY == (taskCB->state & readyState))
    {
        /* @KEEP_COMMENT:
         * 调用ttosClearTaskReady(DT.2.21)清除taskCB任务的就绪状态 */
        (void)ttosClearTaskReady (taskCB);
        /* @KEEP_COMMENT: 设置taskCB任务状态为TTOS_TASK_DORMANT */
        taskCB->state &= TTOS_TASK_EXIT_DEAD | TTOS_TASK_EXIT_ZOMBIE;
        taskCB->state |= (T_UWORD) TTOS_TASK_DORMANT;
    }

    else
    {
        /* @KEEP_COMMENT: 设置taskCB任务状态为TTOS_TASK_DORMANT */
        taskCB->state &= TTOS_TASK_EXIT_DEAD | TTOS_TASK_EXIT_ZOMBIE;
        taskCB->state |= (T_UWORD) TTOS_TASK_DORMANT;
    }
}

/*
 * @brief:
 *    重置任务参数。
 * @param[out]||[in]: taskCB: 任务控制块
 * @return:
 *    无
 * @tracedREQ:
 * @implements: RTE_DTASK.37.2
 */
void ttosInitTaskParam (T_TTOS_TaskControlBlock *taskCB)
{
    taskCB->waitedTicks = 0U;
    /*在任务就绪时，会设置任务的时间片。*/
    taskCB->tickSlice            = 0U;
    taskCB->executedTicks        = 0U;
    taskCB->preemptCount         = taskCB->preemptedConfig;
    taskCB->wait.count           = TTOS_INIT_COUNT;
    taskCB->wait.id              = NULL;
    taskCB->wait.option          = TTOS_INIT_OPTION;
    taskCB->wait.returnArgument  = NULL;
    taskCB->wait.returnArgument1 = NULL;
    taskCB->wait.queue           = NULL;
    taskCB->wait.returnCode      = TTOS_OK;
    taskCB->eventCB              = NULL;
    taskCB->event_wait_node      = LIST_HEAD_INIT(taskCB->event_wait_node);
    taskCB->resourceCount        = 0U;
    taskCB->disableDeleteFlag    = FALSE;
    taskCB->joinedFlag           = FALSE;
    taskCB->taskErrno            = 0;
    taskCB->try_ctx_list         = LIST_HEAD_INIT (taskCB->try_ctx_list);
}

/**
 * @brief:
 *    停止指定的任务。
 * @param[in]: taskID: 任务的ID，当为TTOS_SELF_OBJECT_ID时表示停止当前任务
 * @return:
 *    TTOS_CALLED_FROM_ISR：在中断处理程序中执行此接口。
 *    TTOS_INVALID_USER：<taskID>为TTOS_SELF_OBJECT_ID且当前任务是IDLE任务。
 *    TTOS_INVALID_ID：<taskID>所表示的对象类型不是任务；
 *                     不存在<taskID>指定的任务。
 *    TTOS_INVALID_STATE：<taskID>指定的任务处于休眠态。
 *     TTOS_RESOURCE_IN_USE:
 * <taskID>指定任务拥有互斥信号量或者任务是禁止删除的。
 *    TTOS_OK：停止指定的任务成功。
 * @implements: RTE_DTASK.37.3
 */
T_TTOS_ReturnCode TTOS_StopTask (TASK_ID taskID)
{
    TBSP_MSR_TYPE            msr = 0U;
    T_TTOS_TaskControlBlock *task;
    T_TTOS_TaskControlBlock *executing = ttosGetRunningTask ();

    /* @REPLACE_BRACKET: 在虚拟中断处理程序中执行此接口停止自己*/
    if (TRUE == ttosIsISR ())
    {
        /* @REPLACE_BRACKET: TTOS_CALLED_FROM_ISR */
        return (TTOS_CALLED_FROM_ISR);
    }

    /* @REPLACE_BRACKET: <taskID>等于TTOS_SELF_OBJECT_ID */
    if (TTOS_SELF_OBJECT_ID == taskID)
    {
        /* @REPLACE_BRACKET: 当前任务是IDLE任务 */
        if (TRUE == ttosIsIdleTask (executing))
        {
            /* @REPLACE_BRACKET: TTOS_INVALID_USER */
            return (TTOS_INVALID_USER);
        }

        /* @KEEP_COMMENT: 获取当前运行任务的ID号存放至<taskID> */
        taskID = executing->taskControlId;
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

    /* @REPLACE_BRACKET: 检查任务是否拥有互斥量或者是否是禁止删除的*/
    if ((task->resourceCount > 0U) || (TRUE == task->disableDeleteFlag))
    {
        /* 拥有互斥量的任务不能够被停止，因此恢复调度并返回资源在使用 */
        (void)ttosEnableTaskDispatchWithLock ();
        /* @REPLACE_BRACKET: TTOS_RESOURCE_IN_USE */
        return (TTOS_RESOURCE_IN_USE);
    }

    /* @KEEP_COMMENT: 禁止虚拟中断 */
    TBSP_GLOBALINT_DISABLE (msr);

    /* @REPLACE_BRACKET: task任务状态为TTOS_TASK_DORMANT */
    if (TTOS_TASK_DORMANT == task->state)
    {
        /* @KEEP_COMMENT: 使能虚拟中断 */
        TBSP_GLOBALINT_ENABLE (msr);
        (void)ttosEnableTaskDispatchWithLock ();
        /* @REPLACE_BRACKET: TTOS_INVALID_STATE */
        return (TTOS_INVALID_STATE);
    }

    ttosStopTask (task);
    /* @KEEP_COMMENT: 使能虚拟中断 */
    TBSP_GLOBALINT_ENABLE (msr);
    (void)ttosEnableTaskDispatchWithLock ();
    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}
