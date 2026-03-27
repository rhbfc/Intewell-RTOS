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
T_EXTERN void ttosDisableTaskDispatchWithLock (void);
/************************前向声明******************************/
/************************模块变量******************************/
/************************全局变量******************************/
/************************实   现*******************************/

/* @MODULE> */

/*
 * @brief:
 *   获取任务ID、任务名称、任务优先级、任务状态、任务入口函数、任务等待信息，任务tick数。
 * @param[out]||[in]: task: 任务控制块
 * @param[out]: Info:   任务相关信息
 * @return:
 *    无
 * @implements: RTE_DTASK.17.1
 */
void ttosGetTaskInfo (T_TTOS_TaskControlBlock *task, T_TTOS_TaskInfo *info)
{
    /* @KEEP_COMMENT: 保存获取得的任务信息至<Info> */
    info->id = (T_ULONG)task;
    (void)strcpy ((T_CHAR *)info->name,
                  (T_CONST T_CHAR *)task->objCore.objName);

    /* 此处更改为显示原始优先级 */
    info->taskPriority = task->taskPriority;

    /* @REPLACE_BRACKET: <taskID>为TTOS_SELF_OBJECT_ID ||
     * <taskID>为当前任务的ID号 */
#if CONFIG_SMP == 1
    if (CPU_NONE != task->smpInfo.cpuIndex)
#else
    if (TRUE == ttosIsRunningTask (task))
#endif
    {
        /* @KEEP_COMMENT: 设置<state>为TTOS_TASK_RUNNING */
        info->state = (T_UWORD)TTOS_TASK_RUNNING;
    }
    else
    {
        info->state = task->state;
    }

    info->entry  = task->entry;
    info->option = task->wait.option;
    if (task->wait.id != NULL && task->wait.queue != NULL)
    {
        /* 因为在ttosFlushTaskq()中，会将wait.id设置成TTOS_SELF_OBJECT_ID(0xFFFFFFFF)
         */
        if (task->wait.id != TTOS_SELF_OBJECT_ID)
        {
            ttosCopyName (((T_TTOS_ObjectCoreID)(task->wait.id))->objName,
                          info->waitObjName);
            info->waitObjId = task->wait.id;
        }
        else
        {
            ttosCopyName ((task->objCore.objName), info->waitObjName);
            info->waitObjId = (void *)(&(task->objCore));
        }
    }
    else
    {
        (void)memAlignClear (info->waitObjName, TTOS_OBJECT_NAME_LENGTH + 1U,
                             ALIGNED_BY_ONE_BYTE);
        info->waitObjId = NULL;
    }

    info->executedTicks    = task->executedTicks;
    info->affinityCpuIndex = task->smpInfo.affinityCpuIndex;
    info->cpuIndex         = task->smpInfo.cpuIndex;
}

/**
 * @brief:
 *    获取任务ID、任务名称、任务优先级、任务状态、任务入口函数、任务等待信息，任务tick数。
 * @param[in]:  taskID: 任务ID
 * @param[out]: Info:
 * 任务相关信息，当为TTOS_SELF_OBJECT_ID时表示获取当前任务的信息，
 *                               当为TTOS_IDLE_TASK_ID时表示获取IDLE任务的信息
 * @return:
 *    TTOS_INVALID_ID：<taskID>所表示的对象类型不是任务；
 *                     不存在<taskID>指定的任务。
 *    TTOS_INVALID_ADDRESS：<Info>为NULL。
 *    TTOS_OK：获取任务的状态成功。
 * @implements: RTE_DTASK.17.2
 */
T_TTOS_ReturnCode TTOS_GetTaskInfo (TASK_ID taskID, T_TTOS_TaskInfo *info)
{
    T_TTOS_TaskControlBlock *task = NULL;

    /* @REPLACE_BRACKET: <Info>为空 */
    if (NULL == info)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ADDRESS */
        return (TTOS_INVALID_ADDRESS);
    }

    /* @REPLACE_BRACKET: TTOS_IDLE_TASK_ID == taskID */
    if (TTOS_IDLE_TASK_ID == taskID)
    {
        (void)ttosDisableTaskDispatchWithLock ();
        task = ttosGetIdleTask ();
    }

    else
    {
        /* @KEEP_COMMENT: 获取<taskID>指定的任务存放至变量task */
        task = (T_TTOS_TaskControlBlock *)ttosGetObjectById (taskID,
                                                             TTOS_OBJECT_TASK);

        /* @REPLACE_BRACKET: task任务不存在 */
        if ((0ULL) == task)
        {
            /* @REPLACE_BRACKET: TTOS_INVALID_ID */
            return (TTOS_INVALID_ID);
        }
    }

    ttosGetTaskInfo (task, info);
    (void)ttosEnableTaskDispatchWithLock ();
    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}
