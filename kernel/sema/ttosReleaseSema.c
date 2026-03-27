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
/************************前向声明******************************/
/************************模块变量******************************/
/************************全局变量******************************/
/************************实   现*******************************/

/* @MODULE> */

/**
 * @brief:
 *    释放指定的信号量。任务在释放互斥信号量时会去检测是否有任务因为该信号量而阻塞，
 *如果有就会将该任务的优先级提升到天花板的优先级。
 * @param[in]: semaID: 信号量的ID
 * @return:
 *       TTOS_CALLED_FROM_ISR：互斥信号量在虚拟中断处理程序中执行此接口。
 *       TTOS_NOT_OWNER_OF_RESOURCE：当前任务不是信号量的拥有者
 *    TTOS_INVALID_ID：<semaID>表示的对象类型不是信号量；
 *                     <semaID>指定的信号量不存在。
 *    TTOS_OK：释放信号量成功。
 * @implements: RTE_DSEMA.11.1
 */
T_TTOS_ReturnCode TTOS_ReleaseSemaReturnTask (SEMA_ID semaID, TASK_ID *wakedup_task)
{
    TBSP_MSR_TYPE            msr = 0U;
    T_TTOS_SemaControlBlock *sema;
    T_TTOS_TaskControlBlock *executing;
    T_TTOS_TaskControlBlock *holder;
    T_TTOS_TaskControlBlock *task;
    /* @KEEP_COMMENT: 保存当前任务运行控制块，返回值赋值给executing */
    executing = ttosGetRunningTask ();
    /* @REPLACE_BRACKET: executing为空地址 */
    if (executing == NULL)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ID */
        return (TTOS_INVALID_ID);
    }
    /* @KEEP_COMMENT: 获取<semaID>指定的信号量存放至变量sema */
    sema = (T_TTOS_SemaControlBlock *)ttosGetObjectById (semaID,
                                                         TTOS_OBJECT_SEMA);

    /* @REPLACE_BRACKET: sema为NULL */
    if (NULL == sema)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ID */
        return (TTOS_INVALID_ID);
    }
    /* @REPLACE_BRACKET: 互斥信号量进行一些特殊处理 */
    if (TTOS_MUTEX_SEMAPHORE == (sema->attributeSet & TTOS_SEMAPHORE_MASK))
    {
        /* @REPLACE_BRACKET: 互斥信号量不能够在中断程序中进行释放 */
        if (TRUE == ttosIsISR ())
        {
            /* @KEEP_COMMENT:恢复调度，并返回从中断中调用状态值 */
            (void)ttosEnableTaskDispatchWithLock ();
            /* @REPLACE_BRACKET: TTOS_CALLED_FROM_ISR */
            return (TTOS_CALLED_FROM_ISR);
        }

        /* @REPLACE_BRACKET: 互斥量没有被锁定，说明当前释放者不是拥有者  */
        if (sema->semaControlValue > 0)
        {
            /* @KEEP_COMMENT:恢复调度，并返回不是资源所有者状态值 */
            (void)ttosEnableTaskDispatchWithLock ();
            /* @REPLACE_BRACKET: TTOS_NOT_OWNER_OF_RESOURCE */
            return (TTOS_NOT_OWNER_OF_RESOURCE);
        }

        /* @REPLACE_BRACKET:
         * 嵌套数为零，计数为零，表示创建时计数就是零,在此情况下第一次的释放者就是拥有者
         */
        if (sema->nestCount == 0U)
        {
            /* @KEEP_COMMENT:将拥有者调整为当前释放任务 */
            sema->semHolder = executing->taskControlId;
            sema->nestCount = 1U;
            executing->resourceCount++;
        }

        /* @KEEP_COMMENT: 根据拥有者ID号获取拥有者任务对象 */
        holder = sema->semHolder;

        /* @REPLACE_BRACKET: 互斥信号量的释放者必需是拥有者 */
        if (holder != executing)
        {
            /* @KEEP_COMMENT: 不是拥有者，恢复调度，并返回不是资源所有者状态值
             */
            (void)ttosEnableTaskDispatchWithLock ();
            /* @REPLACE_BRACKET: TTOS_NOT_OWNER_OF_RESOURCE */
            return (TTOS_NOT_OWNER_OF_RESOURCE);
        }

        /* @KEEP_COMMENT: 嵌套深度减一 */
        sema->nestCount--;

        /* @KEEP_COMMENT: 嵌套深度不为零，表示还在嵌套中 */
        /* @REPLACE_BRACKET: sema->nestCount != U(0) */
        if (sema->nestCount != 0U)
        {
            /* @KEEP_COMMENT: 释放信号量 */
            sema->semaControlValue++;
            /* @KEEP_COMMENT: 恢复调度，返回成功 */
            (void)ttosEnableTaskDispatchWithLock ();
            /* @REPLACE_BRACKET: TTOS_OK */
            return (TTOS_OK);
        }

        /* 拥有者资源数减1 */
        /* @REPLACE_BRACKET: executing->resourceCount != 0 */
        if (executing->resourceCount != 0)
        {
            /* @KEEP_COMMENT: 拥有者资源数减1 */
            executing->resourceCount--;
        }

        /* @KEEP_COMMENT: 清空拥有者 */
        sema->semHolder = TTOS_NULL_OBJECT_ID;

        /* @REPLACE_BRACKET:
         * 释放者没有拥有其它资源，并且当前优先级与实际优先级不相等，则恢复任务的实际优先级
         */
        if ((executing->resourceCount == 0U)
            && (holder->taskCurPriority != holder->taskPriority))
        {
            /* @KEEP_COMMENT: 改变任务优先级 */
            (void)taskChangePriority (holder, holder->taskPriority, TRUE);
        }
    }

    /* @KEEP_COMMENT: 禁止虚拟中断 */
    TBSP_GLOBALINT_DISABLE (msr);

    /* @REPLACE_BRACKET: 信号量值小于最大允许值(避免计数信号量溢出) */
    if (sema->semaControlValue < INT_MAX)
    {
        /* @KEEP_COMMENT: 将sema信号量的计数值加1 */
        sema->semaControlValue++;
    }

    task = ttosDequeueTaskq (&(sema->waitQueue));
    if (wakedup_task)
    {
        *wakedup_task = task;
    }

    /* @KEEP_COMMENT:有等待任务，则将信号量赋予该任务 */
    /* @REPLACE_BRACKET: task != NULL */
    if (task != NULL)
    {
        /* @KEEP_COMMENT: 使能虚拟中断 */
        TBSP_GLOBALINT_ENABLE (msr);

        /* @REPLACE_BRACKET:
         * 互斥量的释放不允许在中断中使用，因此可以在开中断情况下进行下面的操作
         */
        if (TTOS_MUTEX_SEMAPHORE == (sema->attributeSet & TTOS_SEMAPHORE_MASK))
        {
            /* @KEEP_COMMENT: 更新嵌套深度和任务的资源使用数 */
            sema->nestCount = 1U;
            /* 信号量的持有者为唤醒的任务 */
            sema->semHolder = task->taskControlId;

            /* 当前任务拥有资源数加1 */
            task->resourceCount++;

            /* @KEEP_COMMENT:
             * 是优先级天花板方式，如果当前优先级低于天花板优先级则调整任务优先级为天花板优先级
             */
            /* @REPLACE_BRACKET: sema->isPriorityCeiling == TRUE */
            if (sema->isPriorityCeiling == TRUE)
            {
                /* @REPLACE_BRACKET: sema->priorityCeiling < task->taskCurPriority
                */
                if (sema->priorityCeiling < task->taskCurPriority)
                {
                    /* @KEEP_COMMENT: 设置当前任务为天花板优先级 */
                    (void)taskChangePriority (task, sema->priorityCeiling, FALSE);
                }
            }
        }
    }

    else
    {
        /* @KEEP_COMMENT: 使能虚拟中断 */
        TBSP_GLOBALINT_ENABLE (msr);
    }

    /* @KEEP_COMMENT: 使能调度 */
    (void)ttosEnableTaskDispatchWithLock ();
    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}

T_TTOS_ReturnCode TTOS_ReleaseSema (SEMA_ID semaID)
{
    return TTOS_ReleaseSemaReturnTask(semaID, NULL);
}
