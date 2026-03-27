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
 *    设置指定周期任务下次启动的延迟时间。
 * @param[in]: taskID:
 * 任务的ID，当为TTOS_SELF_OBJECT_ID时表示设置当前任务的下次启动的延迟时间
 * @param[in]: time: 下次启动的延迟时间
 * @return:
 *    TTOS_INVALID_ID：<taskID>所表示的对象类型不是任务；
 *                     不存在<taskID>指定的任务。
 *    TTOS_INVALID_TYPE：指定的任务不是周期任务。
 *    TTOS_OK：设置周期任务下次启动的延迟时间成功。
 * @notes:
 *    当执行TTOS_ActiveTask()，TTOS_ResetTask()接口时，该接口设置的周期任务下次延迟启动时间<time>会发生作用。
 * @implements: RTE_DTASK.32.1
 */
T_TTOS_ReturnCode TTOS_SetDelayTime (TASK_ID taskID, T_UWORD delay_time)
{
    T_TTOS_TaskControlBlock *task;

    /* @REPLACE_BRACKET: <taskID>等于TTOS_SELF_OBJECT_ID */
    if (TTOS_SELF_OBJECT_ID == taskID)
    {
        /* @KEEP_COMMENT: 获取当前运行任务的ID号存放至<taskID> */
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
    if (TTOS_SCHED_NONPERIOD == task->taskType)
    {
        (void)ttosEnableTaskDispatchWithLock ();
        /* @REPLACE_BRACKET: TTOS_INVALID_TYPE */
        return (TTOS_INVALID_TYPE);
    }

    /* @KEEP_COMMENT: 设置task任务的延迟启动时间为<time> */
    task->periodNode.delayTime = delay_time;
    (void)ttosEnableTaskDispatchWithLock ();
    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}
