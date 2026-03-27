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
#include "commonTypes.h"
#include <ttosBase.h>
#include <ttosInterHal.h>
#include <ttosUtils.inl>
/* @MOD_HEAD> */

/************************宏 定 义******************************/
/************************类型定义******************************/
/************************外部声明******************************/
T_VOID osFaultHook (T_TTOS_HMType type, T_VOID *arg1, T_WORD arg2,
                    T_WORD errorCode);

/* @<MOD_EXTERN */

/* @MOD_EXTERN> */

/************************前向声明******************************/
/************************模块变量******************************/
/************************全局变量******************************/
/************************实   现*******************************/

/* @MODULE> */

/**
 * @brief:
 *    获取指定信号量，当不能立即获得信号量时，根据指定的等待方式等待信号量。
 * @param[in]: semaID: 信号量的ID
 * @param[in]: ticks:
 * 任务等待时间，单位为tick。等待时间为0表示任务不等待信号量；
 *                    <ticks>为TTOS_SEMA_WAIT_FOREVER表示任务永久等待信号量
 * @return:
 *    TTOS_CALLED_FROM_ISR：在虚拟中断处理程序中执行此接口。
 *    TTOS_INVALID_USER：当前任务是IDLE任务。
 *    TTOS_INVALID_ID：<semaID>表示的对象类型不是信号量；
 *                     <semaID>指定的信号量不存在。
 *    TTOS_UNSATISFIED：
 * 当前信号量为互斥信号量且当前任务优先级大于天花板优先级或信号量的计数值为0且任务等待时间为0。
 *    TTOS_TIMEOUT: 信号量的计数值为0且指定等待时间到后任务还没有获得信号量。
 *    TTOS_MUTEX_NESTING_NOT_ALLOWED：当前互斥信号量不支持嵌套获取。
 *    TTOS_MUTEX_CEILING_VIOLATED：任务优先级违反优先级天花板策略，任务优先级高于天花板优先级。
 *    TTOS_MUTEX_NEST_OVERFLOW：互斥信号量超出系统允许嵌套层数。
 *    TTOS_OK：获取信号量成功。
 * @implements: RTE_DSEMA.10.1
 */
T_TTOS_ReturnCode TTOS_ObtainSemaWithInterruptFlags (SEMA_ID semaID, T_UWORD ticks, T_UWORD is_interruptable)
{
    TBSP_MSR_TYPE            msr = 0U;
    T_TTOS_SemaControlBlock *sema;
    T_TTOS_TaskControlBlock *executing;

    /* @REPLACE_BRACKET: 在虚拟中断处理程序中执行此接口 */
    if (TRUE == ttosIsISR ())
    {
        /* @REPLACE_BRACKET: TTOS_CALLED_FROM_ISR */
        return (TTOS_CALLED_FROM_ISR);
    }

    /* @KEEP_COMMENT: 获取当前任务控制块 */
    executing = ttosGetRunningTask ();

    /* @REPLACE_BRACKET: 当前任务是IDLE任务 */
    if (TRUE == ttosIsIdleTask (executing))
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_USER */
        return (TTOS_INVALID_USER);
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

    /* @REPLACE_BRACKET: 是互斥量的优先级天花板则检查当前任务的优先级是否合法 */
    if (sema->isPriorityCeiling == TRUE)
    {
        /* @REPLACE_BRACKET: 当前任务的优先级大于天花板优先级 */
        if (sema->priorityCeiling > executing->taskCurPriority)
        {
            /* @KEEP_COMMENT: 重新使能调度 */
            ttosEnableTaskDispatchWithLock ();

            /* @REPLACE_BRACKET: TTOS_UNSATISFIED */
            return (TTOS_UNSATISFIED);
        }
    }
    /* @KEEP_COMMENT: 禁止虚拟中断 */
    TBSP_GLOBALINT_DISABLE (msr);

    /* @REPLACE_BRACKET: 获取信号量值并判断信号量是否获取成功 */
    if (sema->semaControlValue-- > 0)
    {
        /* @KEEP_COMMENT:  信号量的持有者为当前任务 */
        sema->semHolder = executing->taskControlId;
        /* @KEEP_COMMENT: 信号量已经被获得，可以恢复中断进行后续操作*/
        TBSP_GLOBALINT_ENABLE (msr);

        /* @REPLACE_BRACKET:
         * 互斥量的释放不允许在中断中使用，因此可以在开中断情况下进行下面的操作
         */
        if (TTOS_MUTEX_SEMAPHORE == (sema->attributeSet & TTOS_SEMAPHORE_MASK))
        {
            /* @KEEP_COMMENT: 更新嵌套深度和任务的资源使用数 */
            sema->nestCount = 1;
            executing->resourceCount++;

            /* @KEEP_COMMENT:
             * 是优先级天花板方式，如果当前优先级低于天花板优先级则调整任务优先级为天花板优先级
             */
            /* @REPLACE_BRACKET: sema->isPriorityCeiling == TRUE */
            if (sema->isPriorityCeiling == TRUE)
            {
                /* @REPLACE_BRACKET: 当前任务优先级小于天花板优先级 */
                if (sema->priorityCeiling < executing->taskCurPriority)
                {
                    /* @KEEP_COMMENT: 改变当前任务的优先级 */
                    taskChangePriority (executing, sema->priorityCeiling,
                                        FALSE);
                }
            }
        }
        /* @KEEP_COMMENT: 恢复调度 */
        ttosEnableTaskDispatchWithLock ();
        /* @REPLACE_BRACKET: TTOS_OK */
        return (TTOS_OK);
    }

    /* @REPLACE_BRACKET: 互斥信号量允许同一个任务嵌套拥有 */
    if (TTOS_MUTEX_SEMAPHORE == (sema->attributeSet & TTOS_SEMAPHORE_MASK))
    {
        /* @REPLACE_BRACKET: 当前任务就是互斥量的拥有者，则允许嵌套再次拥有 */
        if (executing->taskControlId == sema->semHolder)
        {
            /* @KEEP_COMMENT: 恢复中断 */
            TBSP_GLOBALINT_ENABLE (msr);

            /* @KEEP_COMMENT: 嵌套深度加1*/
            sema->nestCount++;
            /* @KEEP_COMMENT: 恢复调度 */
            ttosEnableTaskDispatchWithLock ();
            /* @REPLACE_BRACKET: TTOS_OK */
            return (TTOS_OK);
        }
    }

    /* @REPLACE_BRACKET: <ticks>等于0 */
    if (U (0) == ticks)
    {
        /* @KEEP_COMMENT: 使能虚拟中断 */
        TBSP_GLOBALINT_ENABLE (msr);
        ttosEnableTaskDispatchWithLock ();
        /* @REPLACE_BRACKET: TTOS_UNSATISFIED */
        return (TTOS_UNSATISFIED);
    }

    if ((TTOS_MUTEX_SEMAPHORE == (sema->attributeSet & TTOS_SEMAPHORE_MASK) ) &&
    ((sema->attributeSet & TTOS_MUTEX_WAIT_ATTR_MASK) == TTOS_MUTEX_WAIT_PRIORITY_INHERIT) &&
    (sema->semHolder->taskCurPriority > executing->taskCurPriority))
    {
        /* @KEEP_COMMENT: 改变互斥信号量拥有者任务的优先级 */
        taskChangePriority (sema->semHolder,executing->taskCurPriority,FALSE);
    }

    /* @KEEP_COMMENT: 设置当前运行任务资源等待标记为TTOS_OK */
    executing->wait.returnCode = TTOS_OK;

    /* 互斥锁不考虑被信号中断问题，信号量则考虑是否已经有信号可获取 */
    if (!(is_interruptable && pcb_signal_pending((pcb_t)executing->ppcb)))
    {
        /* 互斥锁不可被打断 */
        ttosEnqueueTaskq (&(sema->waitQueue), ticks, is_interruptable);
    }

    if (1 != ttosGetDisableScheduleLevelWithCpuID(cpuid_get()))
    {
        (void)osFaultHook (TTOS_HM_RUNNING_CHECK_EXCEPTION, (T_VOID *)__FUNCTION__,
                           __LINE__, TTOS_TASK_ENTRY_RETURN_ERROR);
    }

    /* @KEEP_COMMENT: 使能虚拟中断 */
    TBSP_GLOBALINT_ENABLE (msr);
    ttosEnableTaskDispatchWithLock ();

    if (executing->wait.returnCode != TTOS_OK)
    {
        ttosDisableTaskDispatchWithLock ();
        TBSP_GLOBALINT_DISABLE (msr);

        sema->semaControlValue ++;

        TBSP_GLOBALINT_ENABLE (msr);
        ttosEnableTaskDispatchWithLock ();
    }

    /* @REPLACE_BRACKET: TTOS_OK */
    return (executing->wait.returnCode);
}
