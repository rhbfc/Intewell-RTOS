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
 *    激活指定的任务。
 * @param[in]: taskID: 任务的ID
 * @return:
 *    TTOS_INVALID_ID：<taskID>所表示的对象类型不是任务；
 *                     不存在<taskID>指定的任务。
 *    TTOS_INVALID_STATE：<taskID>指定的任务不是处于休眠态。
 *    TTOS_OK：激活指定的任务成功。
 * @implements: RTE_DTASK.1.1
 */
T_TTOS_ReturnCode TTOS_ActiveTask (TASK_ID taskID)
{
    TBSP_MSR_TYPE            msr  = 0U;
    T_TTOS_TaskControlBlock *task = NULL;
    /* @KEEP_COMMENT: 获取<taskID>指定的任务存放至变量task */
    task = (T_TTOS_TaskControlBlock *)(ttosGetObjectById (taskID,
                                                          TTOS_OBJECT_TASK));

    /* @REPLACE_BRACKET: task任务不存在 */
    if ((0ULL) == task)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ID */
        return (TTOS_INVALID_ID);
    }

    /* @KEEP_COMMENT: 禁止虚拟中断 */
    TBSP_GLOBALINT_DISABLE (msr);

    /* @REPLACE_BRACKET: task任务状态不为TTOS_TASK_DORMANT */
    if (task->state != (T_UWORD)TTOS_TASK_DORMANT)
    {
        /* @KEEP_COMMENT: 使能虚拟中断 */
        TBSP_GLOBALINT_ENABLE (msr);
        /* 此接口对应ttosGetObjectById中的ttosDisableTaskDispatchWithLock */
        (void)ttosEnableTaskDispatchWithLock ();
        /* @REPLACE_BRACKET: TTOS_INVALID_STATE */
        return (TTOS_INVALID_STATE);
    }

    /* @REPLACE_BRACKET: task任务类型为TTOS_SCHED_PERIOD */
    if (TTOS_SCHED_PERIOD == task->taskType)
    {
        T_UWORD time_temp;

        /* @REPLACE_BRACKET: task任务延迟启动时间不为零 */
        if (task->periodNode.delayTime != 0U)
        {
            /* @KEEP_COMMENT: 设置task任务的周期等待时间为task任务的延迟启动时间
             */
            task->periodNode.waitedTime = task->periodNode.delayTime;
            /* 获取已经等待的时间,因为要将任务插入到周期等待队列中,需要更新第一个任务的等待时间
             */
            /* @KEEP_COMMENT:
             * 调用tbspGetPWaitTick(DT.6.11)获取周期等待队列已经等待的时间 */
            time_temp = tbspGetPWaitTick ();
            /*
             * @KEEP_COMMENT:
             * 调用ttosInsertWaitedPeriodTask(DT.2.25)将任务插入周期等待队列
             * ttosManager.periodWaitedQueue并获取队列被通知时间存放至变量time
             */
            time_temp = ttosInsertWaitedPeriodTask (
                &(task->periodNode), &ttosManager.periodWaitedQueue, time_temp);

            /* @REPLACE_BRACKET: time不等于零 */
            if (0U != time_temp)
            {
                /* @KEEP_COMMENT: 根据time设置周期等待队列的通知时间 */
                (void)tbspSetPWaitTick (time_temp);
            }

            /* @KEEP_COMMENT: 设置task任务状态为(TTOS_TASK_PWAITING |
             * TTOS_TASK_FIRSTRUN) */
            task->state
                = (T_UWORD)TTOS_TASK_PWAITING | (T_UWORD)TTOS_TASK_FIRSTRUN;
        }

        else
        {
            /* @KEEP_COMMENT: 设置task任务的截止期等待时间为task任务的持续时间
             */
            task->periodNode.waitedTime = task->periodNode.durationTime;
            /* 获取已经等待的时间,因为要将任务插入到周期等待队列中,需要更新第一个任务的等待时间
             */
            /* @KEEP_COMMENT:
             * 调用tbspGetPExpireTick(DT.6.12)获取截止期等待队列已等待的时间 */
            time_temp = tbspGetPExpireTick ();
            /*
             * @KEEP_COMMENT:
             * 调用ttosInsertWaitedPeriodTask(DT.2.25)将任务插入截止期等待队列
             * ttosManager.expireWaitedQueue并获取队列被通知时间存放至变量time
             */
            time_temp = ttosInsertWaitedPeriodTask (
                &(task->periodNode), &ttosManager.expireWaitedQueue, time_temp);

            /* @REPLACE_BRACKET: time不等于0 */
            if (U (0) != time_temp)
            {
                /* @KEEP_COMMENT: 根据time设置周期截止期等待队列的通知时间 */
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
    /* 此接口对应ttosGetObjectById中的ttosDisableTaskDispatchWithLock */
    (void)ttosEnableTaskDispatchWithLock ();
    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}
