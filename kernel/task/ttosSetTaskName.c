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
 *    设置指定任务的名字。
 * @param[in]: taskID: 任务ID，当为TTOS_SELF_OBJECT_ID时表示获取当前任务的名字
 * @param[out]: taskName: 任务名字
 * @return:
 *    TTOS_INVALID_ID：<taskID>所表示的对象类型不是任务；
 *                     不存在<taskID>指定的任务。
 *    TTOS_INVALID_ADDRESS：<taskName>为NULL。
 *    TTOS_OK：设置任务的名字成功。
 * @implements: RTE_DTASK.18.1
 */
T_TTOS_ReturnCode TTOS_SetTaskName (TASK_ID taskID, T_UBYTE *taskName)
{
    T_TTOS_TaskControlBlock *task;

    /* @REPLACE_BRACKET: <taskName>为空 */
    if (NULL == taskName)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ADDRESS */
        return (TTOS_INVALID_ADDRESS);
    }

    /* @REPLACE_BRACKET: <taskID>为TTOS_SELF_OBJECT_ID */
    if (TTOS_SELF_OBJECT_ID == taskID)
    {
        /*
         * @KEEP_COMMENT:
         * 调用ttosCopyName(DT.8.2)将当前运行任务ttosManager.runningTask
         * 的名字拷贝至<taskName>
         */
        (void)ttosCopyName (taskName, ttosGetRunningTask ()->objCore.objName);
        /* @REPLACE_BRACKET: TTOS_OK */
        return (TTOS_OK);
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

    /* @KEEP_COMMENT: 调用ttosCopyName(DT.8.2)将获取的任务名拷贝至<taskName> */
    (void)ttosCopyName (taskName, task->objCore.objName);
    (void)ttosEnableTaskDispatchWithLock ();
    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}
