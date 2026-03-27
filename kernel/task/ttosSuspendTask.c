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
 *    挂起任务。
 * @param[out]||[in]: taskCB: 任务控制块
 * @return:
 *    无
 * @implements: RTE_DTASK.38.1
 */
void ttosSuspendTask (T_TTOS_TaskControlBlock *taskCB)
{
    T_UWORD suspendState = (T_UWORD)TTOS_TASK_SUSPEND;
    T_UWORD readyState   = (T_UWORD)TTOS_TASK_READY;
    /* @KEEP_COMMENT: 设置task任务的挂起标记 */
    taskCB->state |= suspendState;
    /* @REPLACE_BRACKET: task任务状态为TTOS_TASK_READY */
    if (readyState == (taskCB->state & readyState))
    {
        /* @KEEP_COMMENT: 调用ttosClearTaskReady(DT.2.21)清除task任务的就绪状态
         */
        (void)ttosClearTaskReady (taskCB);
    }
}

/**
 * @brief:
 *    挂起指定的任务。
 * @param[in]: taskID: 任务的ID，当为TTOS_SELF_OBJECT_ID时表示挂起当前任务
 * @return:
 *    TTOS_CALLED_FROM_ISR：在虚拟中断处理程序中使用TTOS_SELF_OBJECT_ID参数执行此接口。
 *    TTOS_INVALID_USER：<taskID>为TTOS_SELF_OBJECT_ID且当前任务是IDLE任务。
 *    TTOS_INVALID_ID：<taskID>所表示的对象类型不是任务；
 *                     不存在<taskID>指定的任务。
 *    TTOS_INVALID_STATE：<taskID>指定的任务处于休眠或挂起态。
 *    TTOS_OK：挂起指定的任务成功。
 * @implements: RTE_DTASK.38.2
 */
T_TTOS_ReturnCode TTOS_SuspendTask (TASK_ID taskID)
{
    TBSP_MSR_TYPE            msr = 0U;
    T_TTOS_TaskControlBlock *task;
    T_TTOS_TaskControlBlock *executing    = ttosGetRunningTask ();
    T_UWORD                  dormantState = (T_UWORD)TTOS_TASK_DORMANT;
    T_UWORD                  suspendState = (T_UWORD)TTOS_TASK_SUSPEND;
    T_UWORD                  state        = dormantState | suspendState;
    if (NULL == executing)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ID */
        return (TTOS_INVALID_ID);
    }

    /* @REPLACE_BRACKET: <taskID>等于TTOS_SELF_OBJECT_ID */
    if ((TASK_ID)TTOS_SELF_OBJECT_ID == taskID)
    {
        /* @REPLACE_BRACKET: 在虚拟中断处理程序中执行此接口挂起自己*/
        if (TRUE == ttosIsISR ())
        {
            /* @REPLACE_BRACKET: TTOS_CALLED_FROM_ISR */
            return (TTOS_CALLED_FROM_ISR);
        }

        /* @REPLACE_BRACKET: 当前任务是IDLE任务 */
        if (TRUE == ttosIsIdleTask (executing))
        {
            /* @REPLACE_BRACKET: TTOS_INVALID_USER */
            return (TTOS_INVALID_USER);
        }

        /* @KEEP_COMMENT: 获取当前运行任务的ID号存放至<taskID> */
        taskID = executing->taskControlId;
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

    /* @KEEP_COMMENT: 禁止虚拟中断 */
    TBSP_GLOBALINT_DISABLE (msr);

    /* @REPLACE_BRACKET: task任务状态为TTOS_TASK_DORMANT | TTOS_TASK_SUSPEND */
    if (0 != (task->state & state))
    {
        /* @KEEP_COMMENT: 使能虚拟中断 */
        TBSP_GLOBALINT_ENABLE (msr);
        (void)ttosEnableTaskDispatchWithLock ();
        /* @REPLACE_BRACKET: TTOS_INVALID_STATE */
        return (TTOS_INVALID_STATE);
    }
    ttosSuspendTask (task);
    /* @KEEP_COMMENT: 使能虚拟中断*/
    TBSP_GLOBALINT_ENABLE (msr);
    (void)ttosEnableTaskDispatchWithLock ();
    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}

T_TTOS_ReturnCode TTOS_SignalSuspendTask (TASK_ID taskID)
{
    T_TTOS_ReturnCode ret;

    (void)ttosDisableTaskDispatchWithLock ();
    taskID->state |= TTOS_TASK_STOPPED_BY_SIGNAL;
    ret = TTOS_SuspendTask (taskID);
    (void)ttosEnableTaskDispatchWithLock ();

    return (ret);
}


/* 挂起保留task结构体，等待回收僵尸进程处理 */
T_TTOS_ReturnCode task_delete_suspend (TASK_ID taskID)
{
    T_TTOS_ReturnCode ret;

    (void)ttosDisableTaskDispatchWithLock ();
    taskID->state |= TTOS_TASK_WAITING_DELETE;
    ret = TTOS_SuspendTask (taskID);
    (void)ttosEnableTaskDispatchWithLock ();

    return (ret);
}