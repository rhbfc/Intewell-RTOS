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
#include <errno.h>
#include <ttosBase.h>
#include <ttosInterHal.h>
#include <ttosInterTask.inl>
#include <ttosUtils.inl>

#define KLOG_TAG "Kernel"
#include <klog.h>

/* @MOD_HEAD> */

/************************宏 定 义******************************/
/************************类型定义******************************/
/************************外部声明******************************/
/* @<MOD_VAR */
T_EXTERN void ttosDisableTaskDispatchWithLock(void);

/* 处理任务删除自身的任务ID */
TASK_ID ttosDeleteHandlerTaskID;
T_EXTERN T_TTOS_EventControl *del_task_event;

T_VOID osFaultHook(T_TTOS_HMType type, T_VOID *arg1, T_WORD arg2, T_WORD errorCode);
void futex_robust_list_exit(T_TTOS_TaskControlBlock *task);

/************************前向声明******************************/
/************************模块变量******************************/
T_TTOS_FREE_STACK_ROUTINE ttosFreeStackRoutine;

/* @MOD_VAR> */
/************************全局变量******************************/
/************************实   现*******************************/
/* @MODULE> */

/*
 * @brief:
 *      设置任务删除自身或者自身退出时，释放任务栈空间的处理函数。
 * @param[in]: freeStackRoutine: 释放任务栈空间的处理函数
 * @return:
 *    无
 * @notes:
 *      由系统分配的栈空间，在任务退出运行时，才能通过<freeStackRoutine>释放。
 * @implements: RTE_DTASK.5.1
 */
void ttosSetFreeStackRoutine(T_TTOS_FREE_STACK_ROUTINE freeStackRoutine)
{
    /* @KEEP_COMMENT: <ttosFreeStackRoutine>接收释放任务栈空间的处理函数入口地址
     */
    ttosFreeStackRoutine = freeStackRoutine;
}

/*
 * @brief:
 *      处理任务删除自身的任务人口函数
 * @param[in]: arg: 任务入口参数
 * @return:
 *    无
 * @notes:
 *      该接口在任务删除自身的时候会被唤醒
 * @implements: RTE_DTASK.5.2
 */
void taskDeleteSelfHandler(T_VOID *arg)
{
    T_TTOS_TaskControlBlock *deleteSelfTaskCB;
    T_UWORD eventOut;
    struct list_node *pNode;

    /* @REPLACE_BRACKET: 1 */
    while (1)
    {
        /* @KEEP_COMMENT: 等待TTOS_TASK_DELETE_SELF_EVENT事件的到来 */
        (void)TTOS_ReceiveEvent(del_task_event, TTOS_TASK_DELETE_SELF_EVENT, TTOS_EVENT_ALL,
                                TTOS_EVENT_WAIT_FOREVER, &eventOut);

        /* @KEEP_COMMENT:
         *其他任务退出时，仅存在向deleteSelfTaskIdQueue添加节点，此处读访问
         *deleteSelfTaskIdQueue是判断队列是否为空及获取第一个节点，不需要保
         *证原子访问也不影响代码执行的正确性。
         */
        /* @REPLACE_BRACKET:
         * list_is_empty(&(ttosManager.deleteSelfTaskIdQueue)) == FALSE*/
        while (list_is_empty(&(ttosManager.deleteSelfTaskIdQueue)) == FALSE)
        {
            /* @KEEP_COMMENT:
             * 删除自身任务的ID队列不为空，获取任务删除自身队列中的第一个节点 */
            pNode = list_first(&(ttosManager.deleteSelfTaskIdQueue));
            /* @KEEP_COMMENT: 获取任务控制块 */
            deleteSelfTaskCB = (T_TTOS_TaskControlBlock *)(pNode);

#if defined(CONFIG_SMP)
            if (deleteSelfTaskCB == NULL)
            {
                return;
            }

            /* @REPLACE_BRACKET: CPU_NONE != deleteSelfTaskCB->smpInfo.cpuIndex
             */
            if (CPU_NONE != deleteSelfTaskCB->smpInfo.cpuIndex)
            {
                /*
                 *需要等其它核的运行任务不使用栈时，才能在将任务归还系统时释放任务的栈空间。
                 *通过内核锁保证其它核上的运行任务彻底不使用栈时才会释放任务的栈空间。
                 */
                continue;
            }
#endif
            (void)ttosDisableTaskDispatchWithLock();
            /* @KEEP_COMMENT: 将节点从任务删除自身队列中移除 */
            list_delete(pNode);
            /* @KEEP_COMMENT: 将对象归还到系统 */
            (void)ttosReturnObjectToInactiveResource(deleteSelfTaskCB, TTOS_OBJECT_TASK, FALSE);
            /* @KEEP_COMMENT:
             * 使能调度和释放内核锁，避免长时间禁止调度和获取内核锁。*/
            (void)ttosEnableTaskDispatchWithLock();
        }
    }
}

/*
 * @brief:
 *    任务退出。
 * @param[out]||[in]: task: 任务控制块
 * @param[in]: exitValue: 任务退出参数
 * @return:
 *    无
 * @notes:
 *    进入此接口时，是禁止调度的，退出此接口时，是使能调度的。
 *    对于POSIX任务，taskCB一定是当前当前运行任务。
 * @implements: RTE_DTASK.5.4
 */
void ttosTaskExit(T_TTOS_TaskControlBlock *taskCB, T_VOID *exitValue)
{
    TBSP_MSR_TYPE msr = 0U;

    /* @REPLACE_BRACKET: 检查任务是否拥有互斥量或者是否是禁止删除的*/
    if ((taskCB->resourceCount > 0) || (TRUE == taskCB->disableDeleteFlag))
    {
        taskCB->taskErrno = EBUSY;
        (void)ttosSuspendTask(taskCB);
        (void)ttosEnableTaskDispatchWithLock();
        /* @KEEP_COMMENT: 任务退出时，没有成功退出
         * 。有可能是开关调度没有配对使用造成的。*/
        (void)osFaultHook(TTOS_HM_RUNNING_CHECK_EXCEPTION, (T_VOID *)__FUNCTION__, __LINE__,
                          TTOS_TASK_ENTRY_RETURN_ERROR);
        return;
    }

    /* 任务在删除过程中是不允许被删除的*/
    taskCB->disableDeleteFlag = TRUE;

    futex_robust_list_exit(taskCB);

    (void)ttosEnableTaskDispatchWithLock();
    /* @KEEP_COMMENT:调用任务退出函数 */
    (void)TTOS_ExitTaskHook(taskCB->taskControlId);
    (void)ttosDisableTaskDispatchWithLock();

    TBSP_GLOBALINT_DISABLE(msr);

#ifdef CONFIG_PROTECT_STACK
    /* @KEEP_COMMENT: 解除栈保护 */
    ttosStackUnprotect(taskCB->taskControlId);
#endif /* CONFIG_PROTECT_STACK */

    /* @REPLACE_BRACKET: taskCB任务为休眠状态 */
    if (TTOS_TASK_DORMANT != taskCB->state)
    {
        /* 停止任务 */
        (void)ttosStopTask(taskCB);
        /* @KEEP_COMMENT: 删除的任务是正在运行的任务 */
#if defined(CONFIG_SMP)
#if CONFIG_SMP == 1

        /* @REPLACE_BRACKET: CPU_NONE != taskCB->smpInfo.cpuIndex*/
        if (CPU_NONE != taskCB->smpInfo.cpuIndex)
#else

        /* @REPLACE_BRACKET: 删除的任务是当前正在运行的任务 */
        if (taskCB == ttosGetRunningTask())
#endif
#else  /*defined(CONFIG_SMP)*/
        /* @REPLACE_BRACKET: 删除的任务是当前正在运行的任务 */
        if (taskCB == ttosGetRunningTask())
#endif /*defined(CONFIG_SMP)*/
        {
            /* @KEEP_COMMENT: 使对象节点无效 */
            ttosInvalidObjectCore((T_TTOS_ObjectCore *)taskCB);
            /* @KEEP_COMMENT:
             * 将要删除的任务节点存放到任务删除自己的结构队列链表中 */
            list_add_tail(&(taskCB->objCore.objectNode),
                          &(ttosManager.deleteSelfTaskIdQueue));
            /* @KEEP_COMMENT:
             * 向处理任务删除自身的任务发送事件TTOS_TASK_DELETE_SELF_EVENT*/
            if (ttosDeleteHandlerTaskID != NULL)
            {
                (void)TTOS_SendEvent(del_task_event, TTOS_TASK_DELETE_SELF_EVENT);
            }
            /* @KEEP_COMMENT: 使能中断 */
            TBSP_GLOBALINT_ENABLE(msr);
            /*正常情况下，当中断到来导致任务被切走时，当前运行任务由于已经不在就绪队列上，不会再切回来。*/
            /* @KEEP_COMMENT: 重新使能调度开关 */
            (void)ttosEnableTaskDispatchWithLock();
            /* @KEEP_COMMENT: 任务退出时，没有成功退出
             * 。有可能是开关调度没有配对使用造成的。*/
            (void)osFaultHook(TTOS_HM_RUNNING_CHECK_EXCEPTION, (T_VOID *)__FUNCTION__, __LINE__,
                              TTOS_TASK_ENTRY_RETURN_ERROR);
            return;
        }
    }

    /* @KEEP_COMMENT: 使能中断 */
    TBSP_GLOBALINT_ENABLE(msr);
    (void)ttosReturnObjectToInactiveResource(taskCB, TTOS_OBJECT_TASK, FALSE);
    (void)ttosEnableTaskDispatchWithLock();
}

/**
 * @brief
 *      删除任务。
 * @param[in]  taskID:
 * 要删除的任务标识符，当为TTOS_SELF_OBJECT_ID时表示删除当前任务
 * @return
 *       TTOS_CALLED_FROM_ISR: 从中断中调用。
 *       TTOS_INVALID_USER：<taskID>为TTOS_SELF_OBJECT_ID且当前任务是IDLE任务。
 *       TTOS_INVALID_ID: 无效的任务标识符。
 *       TTOS_RESOURCE_IN_USE: 指定任务拥有互斥信号量或者任务是禁止删除的。
 *       TTOS_OK: 成功删除指定任务。
 * @implements: RTE_DTASK.5.5
 */
T_TTOS_ReturnCode TTOS_DeleteTask(TASK_ID taskID)
{
    T_TTOS_TaskControlBlock *taskCB;
    T_TTOS_TaskControlBlock *task = ttosGetRunningTask();
    if ((0ULL) == task)
    {
        return (TTOS_INVALID_ID);
    }

    /* @REPLACE_BRACKET: TRUE == ttosIsISR()*/
    if (TRUE == ttosIsISR())
    {
        /* @REPLACE_BRACKET: TTOS_CALLED_FROM_ISR*/
        return (TTOS_CALLED_FROM_ISR);
    }

    /* @REPLACE_BRACKET: <taskID>等于TTOS_SELF_OBJECT_ID */
    if (TTOS_SELF_OBJECT_ID == taskID)
    {
        /* @REPLACE_BRACKET: 当前任务是IDLE任务 */
        if (TRUE == ttosIsIdleTask(task))
        {
            /* @REPLACE_BRACKET: TTOS_INVALID_USER */
            return (TTOS_INVALID_USER);
        }

        /* @KEEP_COMMENT: 获取当前运行任务的ID号存放至<taskID> */
        taskID = task->taskControlId;
    }

    /* @KEEP_COMMENT: 获取<taskID>指定的任务存放至变量taskCB */
    taskCB = (T_TTOS_TaskControlBlock *)(ttosGetObjectById(taskID, TTOS_OBJECT_TASK));

    /* @REPLACE_BRACKET: task任务不存在 */
    if (NULL == taskCB)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ID */
        return (TTOS_INVALID_ID);
    }

    /* @KEEP_COMMENT: 检查任务是否拥有互斥量或者是否是禁止删除的*/
    /* @REPLACE_BRACKET: (taskCB->resourceCount > U(0)) || (TRUE ==
     * taskCB->disableDeleteFlag) */
    if (TRUE == taskCB->disableDeleteFlag)
    {
        /* @KEEP_COMMENT:
         * 拥有互斥量的任务不能够被删除，因此恢复调度并返回资源在使用 */
        (void)ttosEnableTaskDispatchWithLock();
        /* @REPLACE_BRACKET: TTOS_RESOURCE_IN_USE */
        return (TTOS_RESOURCE_IN_USE);
    }

    if (taskCB->resourceCount > 0U)
    {
        KLOG_E("task exit with lock");
    }

    ttosTaskExit(taskCB, NULL);
    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}
