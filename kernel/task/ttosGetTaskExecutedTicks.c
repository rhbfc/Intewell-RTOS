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
 *    获取指定任务已经执行的tick数。
 * @param[in]: taskID:
 * 任务ID，当为TTOS_SELF_OBJECT_ID时表示获取当前任务已经执行的tick数
 * @param[out]: executedTicks: 存放任务已经执行的tick数
 * @return:
 *    TTOS_INVALID_ID：<taskID>所指定的对象类型不是任务或不存在。
 *    TTOS_INVALID_ADDRESS：<executedTicks>为NULL。
 *    TTOS_OK：获取任务的已经执行的tick数成功。
 * @implements: RTE_DTASK.14.1
 */
T_TTOS_ReturnCode TTOS_GetTaskExecutedTicks (TASK_ID   taskID,
                                             T_UDWORD *executedTicks)
{
    T_TTOS_TaskControlBlock *task;

    /* @REPLACE_BRACKET: <executedTicks>为空 */
    if (NULL == executedTicks)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ADDRESS */
        return (TTOS_INVALID_ADDRESS);
    }

    /* @REPLACE_BRACKET: <taskID>为TTOS_SELF_OBJECT_ID */
    if (TTOS_SELF_OBJECT_ID == taskID)
    {
        /* @KEEP_COMMENT: 将当前任务的tick数赋值给<executedTicks>  */
        *executedTicks = ttosGetRunningTask ()->executedTicks;
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

    else
    {
        /* @KEEP_COMMENT: 将指定任务运行的tick数赋值给<executedTicks>  */
        *executedTicks = task->executedTicks;
    }

    /* @KEEP_COMMENT:使能调度 */
    (void)ttosEnableTaskDispatchWithLock ();
    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}

/**
 * @brief:
 *    获取所有CPU上idle任务运行的tick数。
 * @param[out]: ticks: 存放idle任务运行tick数的变量
 * @return:
 *    TTOS_OK:获取系统tick成功。
 * @implements: RTE_DTASK.14.2
 */
T_TTOS_ReturnCode TTOS_GetIdleTaskExecutedTicks (T_UDWORD *ticks)
{
    T_UWORD  i;
    T_UWORD  enabledNum       = cpuEnabledNumGet ();
    T_UDWORD allIdleTaskTicks = 0UL;

    /* @REPLACE_BRACKET: i = U(0); i < enabledNum; i++ */
    for (i = U (0); i < enabledNum; i++)
    {
        allIdleTaskTicks += ttosGetIdleTaskWithCpuID (i)->executedTicks;
    }

    *ticks = allIdleTaskTicks;
    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}

/**
 * @brief:
 *    获取指定CPU上idle任务运行的tick数。
 * @param[in]: cpuID: CPU ID，仅仅多核才使用此参数
 * @param[out]: ticks: 存放idle任务运行tick数的变量
 * @return:
 *    TTOS_OK:获取系统tick成功。
 * @implements: RTE_DTASK.14.3
 */
T_TTOS_ReturnCode TTOS_GetIdleTaskExecutedTicksWithCpuID (T_UWORD   cpuID,
                                                          T_UDWORD *ticks)
{
    *ticks = ttosGetIdleTaskWithCpuID (cpuID)->executedTicks;
    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}

/**
 * @brief:
 *    设置指定任务已经执行的tick数。
 * @param[in]: taskID:
 * 任务ID，当为TTOS_SELF_OBJECT_ID时表示获取当前任务已经执行的tick数
 * @param[in]: executedTicks: 存放任务已经执行的tick数
 * @return:
 *    TTOS_INVALID_ID：<taskID>所指定的对象类型不是任务或不存在。
 *    TTOS_INVALID_ADDRESS：<executedTicks>为NULL。
 *    TTOS_OK：获取任务的已经执行的tick数成功。
 * @implements: RTE_DTASK.14.4
 */
T_TTOS_ReturnCode TTOS_SetTaskExecutedTicks (TASK_ID  taskID,
                                             T_UDWORD executedTicks)
{
    T_TTOS_TaskControlBlock *task;
    TBSP_MSR_TYPE            msr = 0U;

    /* @REPLACE_BRACKET: <executedTicks>为空 */
    if (((T_UDWORD)0UL) == executedTicks)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ADDRESS */
        return (TTOS_INVALID_ADDRESS);
    }

    /* @REPLACE_BRACKET: <taskID>为TTOS_SELF_OBJECT_ID */
    if (TTOS_SELF_OBJECT_ID == taskID)
    {
        /* @KEEP_COMMENT: 禁止虚拟中断 */
        TBSP_GLOBALINT_DISABLE (msr);
        ttosGetRunningTask ()->executedTicks = executedTicks;
        /* @KEEP_COMMENT: 使能虚拟中断 */
        TBSP_GLOBALINT_ENABLE (msr);
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

    else
    {
        /* @KEEP_COMMENT: 禁止虚拟中断 */
        TBSP_GLOBALINT_DISABLE (msr);
        /* @KEEP_COMMENT: 将指定任务运行的tick数赋值给<executedTicks>  */
        task->executedTicks = executedTicks;
        /* @KEEP_COMMENT: 使能虚拟中断 */
        TBSP_GLOBALINT_ENABLE (msr);
    }

    /* @KEEP_COMMENT:使能调度 */
    (void)ttosEnableTaskDispatchWithLock ();
    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}
