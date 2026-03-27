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
int period_sched_group_work_complete (void);

/* @<MOD_EXTERN */

/* @MOD_EXTERN> */

/************************前向声明******************************/
/************************模块变量******************************/
/************************全局变量******************************/
/************************实   现*******************************/

/* @MODULE> */

/**
 * @brief:
 *    使当前周期任务等待下个周期到来。
 * @return:
 *    TTOS_CALLED_FROM_ISR：在虚拟中断处理程序中执行此接口。
 *    TTOS_INVALID_TYPE：当前任务不是周期任务。
 *    TTOS_OK：使当前周期任务等待下个周期成功。
 * @implements: RTE_DTASK.42.1
 */
T_TTOS_ReturnCode TTOS_WaitPeriod (void)
{
    TBSP_MSR_TYPE            msr = 0;
    T_TTOS_TaskControlBlock *task;
    T_UWORD                  state = (T_UWORD)TTOS_TASK_CWAITING;

    /* @REPLACE_BRACKET: 在虚拟中断处理程序中执行此接口 */
    if (TRUE == ttosIsISR ())
    {
        /* @REPLACE_BRACKET: TTOS_CALLED_FROM_ISR */
        return (TTOS_CALLED_FROM_ISR);
    }

    /* @KEEP_COMMENT: 获取当前正在运行的任务存放至变量task */
    task = ttosGetRunningTask ();

    /* @REPLACE_BRACKET: task任务不是周期任务 */
    if (TTOS_SCHED_NONPERIOD == task->taskType)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_TYPE */
        return (TTOS_INVALID_TYPE);
    }

    /* @KEEP_COMMENT: 禁止虚拟中断 */
    TBSP_GLOBALINT_DISABLE_WITH_LOCK (msr);
    /* @KEEP_COMMENT: 设置task任务工作完成标志为TRUE */
    task->periodNode.jobCompleted = TRUE;
    /* @KEEP_COMMENT: 将task任务的状态设置为当前状态与TTOS_TASK_CWAITING的复合态
     */
    task->state |= state;
    
    period_sched_group_work_complete();
	
    /* @KEEP_COMMENT: 调用ttosClearTaskReady(DT.2.21)清除task任务的就绪状态 */
    (void)ttosClearTaskReady (task);
    (void)ttosScheduleInIntDisAndKernelLock (msr);
    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}
