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
#include <ttosInterTask.inl>
#include <ttosUtils.inl>
/* @MOD_HEAD> */

/************************宏 定 义******************************/
/************************类型定义******************************/
/************************外部声明******************************/
/************************前向声明******************************/
/************************模块变量******************************/
/************************全局变量******************************/
/************************实   现*******************************/

/* @MODULE> */

/**
 * @brief:
 *    获取指定任务的状态。
 * @param[in]: taskID: 任务ID，当为TTOS_SELF_OBJECT_ID时表示获取当前任务的状态
 * @param[out]: state: 任务状态
 * @return:
 *    TTOS_INVALID_ID：<taskID>所表示的对象类型不是任务；
 *                     不存在<taskID>指定的任务。
 *    TTOS_INVALID_ADDRESS：<state>为NULL。
 *    TTOS_OK：获取任务的状态成功。
 * @implements: DT.2.14
 */
T_TTOS_ReturnCode TTOS_GetTaskState (TASK_ID taskID, T_TTOS_TaskState *state)
{
    T_TTOS_TaskControlBlock *task;

    /* @REPLACE_BRACKET: <state>为空 */
    if (NULL == state)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ADDRESS */
        return (TTOS_INVALID_ADDRESS);
    }

    /* @REPLACE_BRACKET: <taskID>为TTOS_SELF_OBJECT_ID ||
     * <taskID>为当前任务的ID号 */
    if ((TTOS_SELF_OBJECT_ID == taskID)
        || (taskID == (ttosGetRunningTask ()->taskControlId)))
    {
        /* @KEEP_COMMENT: 设置<state>为TTOS_TASK_RUNNING */
        *state = TTOS_TASK_RUNNING;

        /* @REPLACE_BRACKET: TTOS_OK */
        return (TTOS_OK);
    }

    /* @KEEP_COMMENT: 获取<taskID>指定的任务存放至变量task */
    task = (T_TTOS_TaskControlBlock *)ttosGetObjectById (taskID,
                                                         TTOS_OBJECT_TASK);

    /* @REPLACE_BRACKET: task任务不存在 */
    if (NULL == task)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ID */
        return (TTOS_INVALID_ID);
    }

    /* @KEEP_COMMENT: 保存获取得的任务状态至<state> */
    *state = task->state;

    ttosEnableTaskDispatchWithLock ();

    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}
