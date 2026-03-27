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
 *    延迟指定周期任务的截止期。
 * @param[in]: taskID:
 * 任务的ID，当为TTOS_SELF_OBJECT_ID时表示延迟当前任务的截止期
 * @param[in]: time: 延迟时间
 * @return:
 *    TTOS_INVALID_ID：<taskID>所表示的对象类型不是任务；
 *                     不存在<taskID>指定的任务。
 *    TTOS_INVALID_TYPE：指定任务不是周期任务。
 *    TTOS_INVALID_TIME：如果延迟后的任务截止时间大于任务周期时间。
 *    TTOS_INVALID_STATE：<taskID>指定的任务处于休眠态或者是周期等待状态。
 *    TTOS_OK：延迟指定周期任务的截止期成功。
 * @notes:
 *    延迟的周期任务的截止期只在该任务的当前周期内有效。
 * @implements: RTE_DTASK.28.1
 */
T_TTOS_ReturnCode TTOS_ReplenishTask (TASK_ID taskID, T_UWORD delay_time)
{
    TBSP_MSR_TYPE            msr = 0U;
    T_TTOS_TaskControlBlock *task;
    T_UWORD                  exactTime;
    T_UWORD                  insertTime;
    T_UWORD                  elapseTime;
    T_UWORD                  newWaitedTime;
    T_UWORD                  doemantState = (T_UWORD)TTOS_TASK_DORMANT;
    T_UWORD                  pwaitState   = (T_UWORD)TTOS_TASK_PWAITING;
    T_UWORD                  state        = doemantState | pwaitState;

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

    /* @REPLACE_BRACKET: task任务不是周期任务 */
    if (task->taskType != TTOS_SCHED_PERIOD)
    {
        (void)ttosEnableTaskDispatchWithLock ();
        /* @REPLACE_BRACKET: TTOS_INVALID_TYPE */
        return (TTOS_INVALID_TYPE);
    }

    /*
     * @KEEP_COMMENT:
     * 延迟后的task任务截止时间为task任务的持续时间加上task任务的已补偿
     * 时间加上此次补偿时间<time>
     */
    /* @REPLACE_BRACKET: 延迟后的task任务截止时间大于task任务周期时间 */
    if ((task->periodNode.durationTime + task->periodNode.replenishTime
         + delay_time)
        > (task->periodNode.periodTime))
    {
        (void)ttosEnableTaskDispatchWithLock ();
        /* @REPLACE_BRACKET: TTOS_INVALID_TIME */
        return (TTOS_INVALID_TIME);
    }

    /* @KEEP_COMMENT: 禁止虚拟中断 */
    TBSP_GLOBALINT_DISABLE (msr);

    /* @REPLACE_BRACKET: task任务状态是TTOS_TASK_DORMANT | TTOS_TASK_PWAITING */
    if (0 != (task->state & state))
    {
        /* @KEEP_COMMENT: 使能虚拟中断 */
        TBSP_GLOBALINT_ENABLE (msr);
        (void)ttosEnableTaskDispatchWithLock ();
        /* @REPLACE_BRACKET: TTOS_INVALID_STATE */
        return (TTOS_INVALID_STATE);
    }

    /* @KEEP_COMMENT: task任务已补偿时间加上<time>进行时间补偿 */
    task->periodNode.replenishTime += delay_time;
    /*
     * @KEEP_COMMENT:
     * 调用tbspGetPExpireTick(DT.6.12)获取周期截止期等待队列已经等待的
     * 时间存放至变量elapseTime
     */
    elapseTime = tbspGetPExpireTick ();
    /*
     * @KEEP_COMMENT: task任务的持续时间减去elapseTime并加上补偿时间存放至变量
     * newWaitedTime
     */
    newWaitedTime = task->periodNode.durationTime - elapseTime
                    + task->periodNode.replenishTime;
    /*
     * @KEEP_COMMENT:
     * 调用ttosExactWaitedPeriodTask(DT.2.23)将task任务从周期任务截止期
     * 等待队列ttosManager.expireWaitedQueue移除并返回截止期等待队列下次被通知
     * 的时间存放至变量exactTime
     */
    /* 将节点从等待队列中移出，并返回第1个节点需要的计时时间 */
    exactTime = ttosExactWaitedPeriodTask (
        &task->periodNode, &ttosManager.expireWaitedQueue, elapseTime);
    /* @KEEP_COMMENT: 重新设定task任务的截止期等待时间为newWaitedTime */
    task->periodNode.waitedTime = newWaitedTime;

    /*
     *exactTime不为0情况下，说明被摘除的<taskID>节点为第一个节点，流失时间已经更新了，
     *因此再插入时就不需要再减去截止期的流失时间了
     */

    /* @REPLACE_BRACKET: exactTime不等于零 */
    if (0U != exactTime)
    {
        /* @KEEP_CODE: */
        elapseTime = 0U;
    }

    /*
     * @KEEP_COMMENT:
     * 将task任务插入周期截止期等待队列，并返回插入后截止期等待队列
     * 下次被通知的时间存放至变量insertTime
     */
    insertTime = ttosInsertWaitedPeriodTask (
        &task->periodNode, &ttosManager.expireWaitedQueue, elapseTime);

    /*只要截止期等待队列的的第一个节点变了，就需要同步更新截止期等待队列的通知时间。*/

    /* @REPLACE_BRACKET: insertTime不等于零 */
    if (0U != insertTime)
    {
        /*
         *insertTime不为0情况下，说明被插入的<taskID>节点为第一个节点，所以需要同步更新截止期等待队列的
         *通知时间。
         */
        /* @KEEP_COMMENT: 根据insertTime设置周期截止期等待队列的通知时间 */
        (void)tbspSetPExpireTick (insertTime);
    }

    else
    {
        /* @REPLACE_BRACKET: exactTime不等于零 */
        if (0U != exactTime)
        {
            /*
             *exactTime不为0情况下，说明被摘除的<taskID>节点为第一个节点，所以需要同步更新截止期等待队列的
             *通知时间。
             */
            /* @KEEP_COMMENT: 根据exactTime调用设置周期截止期等待队列的通知时间
             */
            (void)tbspSetPExpireTick (exactTime);
        }
    }

    /* @KEEP_COMMENT: 使能虚拟中断 */
    TBSP_GLOBALINT_ENABLE (msr);
    (void)ttosEnableTaskDispatchWithLock ();
    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}
