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
#include <trace.h>
#include <ttosBase.h>
#include <ttosInterHal.h>
#include <ttosInterTask.inl>
#include <ttosProcess.h>
#include <ttosTypes.h> /* 解决TTOS_MINIMUM_STACK_SIZE找不到 */
#include <ttosUtils.inl>

#define KLOG_TAG "TASK"
#include <klog.h>

/* @MOD_HEAD> */

/************************宏 定 义******************************/
/************************类型定义******************************/
/************************外部声明******************************/

/* @<MOD_EXTERN */
T_EXTERN T_VOID ttosInitTaskSignal(T_TTOS_TaskControlBlock *taskCB);
T_EXTERN void ttosDisableTaskDispatchWithLock(void);
/*是否栈填充*/
/* @MOD_EXTERN> */
T_EXTERN T_BOOL ttosTaskStackFilledFlag;

/************************前向声明******************************/
/************************模块变量******************************/
/************************全局变量******************************/
/************************实   现*******************************/
/* @MODULE> */

/*
 * @brief:
 *    初始化任务。
 * @param[out]||[in]: taskCB: 任务控制块
 * @param[in]: taskConfig: 任务配置参数
 * @param[out]: taskID: 创建的任务标识符
 * @return:
 *    无
 * @implements: RTE_DTASK.4.1
 */
void ttosInitTask(T_TTOS_TaskControlBlock *taskCB, T_TTOS_ConfigTask *taskConfig, TASK_ID *taskID)
{
    T_UBYTE *lTaskStack;
    TBSP_MSR_TYPE msr = 0U;
    T_UWORD readyState = (T_UWORD)TTOS_TASK_READY;
    T_UWORD firstRunState = (T_UWORD)TTOS_TASK_FIRSTRUN;
    T_UWORD pwaitState = (T_UWORD)TTOS_TASK_PWAITING;
    T_UWORD state = readyState | firstRunState;
    /* @KEEP_COMMENT: 根据配置表的属性初始化指定任务 */
    lTaskStack = taskConfig->taskStack;
    (void)ttosInitTaskParam(taskCB);
    memset(&taskCB->periodNode, 0, sizeof(taskCB->periodNode));
    taskCB->objCore.objectNode.next = NULL;
    taskCB->taskControlId = (TASK_ID)taskCB;
    (void)ttosCopyName(taskConfig->cfgTaskName, taskCB->objCore.objName);
    taskCB->taskPriority = taskConfig->taskPriority;
    taskCB->taskCurPriority = taskConfig->taskPriority;
    taskCB->preemptCount = (taskConfig->preempted == TRUE) ? 0U : 1U;
    taskCB->preemptedConfig = taskCB->preemptCount;
    taskCB->arg = taskConfig->arg;
    taskCB->entry = taskConfig->entry;
    taskCB->taskType = taskConfig->taskType;
    taskCB->ppcb = NULL;
    taskCB->periodNode.periodTime = taskConfig->periodTime;
    taskCB->periodNode.durationTime = taskConfig->durationTime;
    taskCB->periodNode.delayTime = taskConfig->delayTime;
    taskCB->periodNode.task = &(taskCB->objCore.objectNode);
    taskCB->periodNode.objectNode.next = NULL;
    taskCB->resourceNode.task = &(taskCB->objCore.objectNode);
    taskCB->resourceNode.objectNode.next = NULL;
    taskCB->tickSliceSize = taskConfig->tickSliceSize;
    taskCB->executedTicks = 0U;
    taskCB->resourceCount = 0U;
    taskCB->disableDeleteFlag = FALSE;
    taskCB->joinedFlag = FALSE;
    /* @KEEP_COMMENT: 对栈顶进行8字节对齐处理，并添加安全保留空间 */
    taskCB->stackStart = (T_UBYTE *)lTaskStack;
    taskCB->initialStackSize = taskConfig->stackSize;
    taskCB->stackBottom = (T_UBYTE *)lTaskStack;
    taskCB->kernelStackTop = (T_UBYTE *)(((T_ULONG)lTaskStack + (T_ULONG)taskConfig->stackSize -
                                          (T_ULONG)TTOS_SAFE_STACK_RESERVE_SIZE + 7UL) &
                                         (~7UL));
    taskCB->switchContext.sp = (T_ULONG)taskCB->kernelStackTop;
    taskCB->isFreeStack = taskConfig->isFreeStack;

    /* @REPLACE_BRACKET: 当前CPU正在运行任务是否为NULL*/
    if (NULL == ttosGetRunningTask())
    {
        /* @KEEP_COMMENT: 系统初始化过程中创建的任务是绑定运行在核0上的*/
        T_UWORD cpuID = cpuid_get();
        CPU_SET(cpuID, &taskCB->smpInfo.affinity);
        taskCB->smpInfo.affinityCpuIndex = cpuid_get();
    }

    else
    {
        /* x86-64
         * 在开O2优化的情况下,会使用XMM寄存器，此时，此指令要求16字节对齐,&taskCB->smpInfo.affinity未16字节对齐,因此会产生异常
         */
        // taskCB->smpInfo.affinity = ttosGetRunningTask ()->smpInfo.affinity;
        memcpy(&taskCB->smpInfo.affinity, &ttosGetRunningTask()->smpInfo.affinity,
               sizeof(taskCB->smpInfo.affinity));
        taskCB->smpInfo.affinityCpuIndex = ttosGetRunningTask()->smpInfo.affinityCpuIndex;
    }

    taskCB->smpInfo.cpuIndex = CPU_NONE;
    taskCB->smpInfo.last_running_cpu = CPU_NONE;
    taskCB->smpInfo.isChangedToPrivate = FALSE;
    CPU_ZERO(&taskCB->smpInfo.gRunTaskAffinity);
    taskCB->smpInfo.gRunTaskAffinityCpuIndex = CPU_NONE;
    taskCB->smpInfo.isRotated = FALSE;

    spin_lock_init(&taskCB->held_rt_mutexs_lock);
    INIT_LIST_HEAD(&taskCB->held_rt_mutexs);
    taskCB->blocked_on = NULL;
    taskCB->robust_list_head = NULL;
    taskCB->robust_list_len = 0;

    if (taskConfig->pcb)
    {
        ((pcb_t)taskConfig->pcb)->taskControlId = taskCB;
        taskCB->ppcb = taskConfig->pcb;
    }

    if (taskConfig->isprocess)
    {
        tid_obj_alloc_with_id(taskCB, get_process_pid((pcb_t)taskConfig->pcb));
        trace_object(TRACE_OBJ_PROCESS, taskCB);
    }
    else
        tid_obj_alloc(taskCB->taskControlId);

    trace_task_bindcpu(taskCB);

    /* 周期任务 */
    if (TTOS_SCHED_PERIOD == taskCB->taskType)
    {
        /* 设置调度策略为SCHED_FIFO */
        taskCB->taskSchedPolicy = SCHED_FIFO;
    }
    else
    {
        /* 继承创建者的调度策略 */
        taskCB->taskSchedPolicy = ttosGetRunningTask()->taskSchedPolicy;
    }

    (void)ttosDisableTaskDispatchWithLock();
    /* @KEEP_COMMENT: 禁止中断 */
    TBSP_GLOBALINT_DISABLE(msr);

    /* @REPLACE_BRACKET: 任务i自动启动 */
    if (TRUE == taskConfig->autoStarted)
    {
        /* @KEEP_COMMENT: 设置任务i时间片大小 */
        taskCB->tickSlice = taskCB->tickSliceSize;

        /* @REPLACE_BRACKET: 任务i为周期任务 */
        if (TTOS_SCHED_PERIOD == taskCB->taskType)
        {
            /* 有周期启动延迟的，先进行延迟等待 */
            /* @REPLACE_BRACKET: 任务i启动延迟时间不为0 */
            if (taskCB->periodNode.delayTime != 0U)
            {
                /* @KEEP_COMMENT: 设置任务i的等待时间为周期任务启动延迟时间 */
                taskCB->periodNode.waitedTime = taskCB->periodNode.delayTime;
                /*
                 * @KEEP_COMMENT: 调用ttosInsertWaitedPeriodTask(DT.2.25)将任务i
                 * 插入到周期等待队列
                 */
                (void)ttosInsertWaitedPeriodTask(&(taskCB->periodNode),
                                                 &ttosManager.periodWaitedQueue, 0U);
                /* @KEEP_COMMENT: 设置任务i的状态为(周期等待态|首次运行态) */
                taskCB->state = pwaitState | firstRunState;
            }

            else
            {
                /* @KEEP_COMMENT: 设置任务i的等待时间为任务i持续时间 */
                taskCB->periodNode.waitedTime = taskCB->periodNode.durationTime;
                /*
                 * @KEEP_COMMENT: 调用ttosInsertWaitedPeriodTask(DT.2.25)将任务i
                 * 插入到截止期等待队列
                 */
                (void)ttosInsertWaitedPeriodTask(&(taskCB->periodNode),
                                                 &ttosManager.expireWaitedQueue, U(0));
                /* @KEEP_COMMENT: 设置任务i的状态为(就绪态|首次运行态) */
                taskCB->state = state;
                /* @KEEP_COMMENT:
                 * 调用list_add_tail(DT.8.7)加入任务优先级为i的就绪队列 */
                ttosReadyQueuePut(taskCB, FALSE);
                /* @KEEP_COMMENT: 加入优先级位图 */
                TBSP_SET_PRIORITYBITMAP(taskCB->smpInfo.affinityCpuIndex, taskCB->taskCurPriority);
                ttosSetHighestPriorityTask(taskCB->smpInfo.affinityCpuIndex);
            }
        }

        else
        {
            /* 非周期任务自启动 */
            /* @KEEP_COMMENT: 设置任务i状态为(就绪态|首次运行态) */
            taskCB->state = state;
            /* @KEEP_COMMENT:
             * 调用list_add_tail(DT.8.7)加入任务优先级为i级的就绪队列 */
            ttosReadyQueuePut(taskCB, FALSE);
            /* @KEEP_COMMENT: 加入优先级位图 */
            TBSP_SET_PRIORITYBITMAP(taskCB->smpInfo.affinityCpuIndex, taskCB->taskCurPriority);
            ttosSetHighestPriorityTask(taskCB->smpInfo.affinityCpuIndex);
        }
    }

    else
    {
        /* @KEEP_COMMENT: 设置任务i状态为休眠态 */
        taskCB->state = (T_UWORD)TTOS_TASK_DORMANT;
    }

    /* @REPLACE_BRACKET: 任务类型是否为TTOS_SCHED_PERIOD*/
    if (TTOS_SCHED_PERIOD == taskConfig->taskType)
    {
        ttosSetPWaitAndExpireTick();
    }

    /* @KEEP_COMMENT: 使能中断 */
    TBSP_GLOBALINT_ENABLE(msr);
    (void)ttosInitObjectCore(taskCB, TTOS_OBJECT_TASK);

    /* @REPLACE_BRACKET: NULL != taskID*/
    if (NULL != taskID)
    {
        *taskID = taskCB->taskControlId;
    }

#ifdef CONFIG_PROTECT_STACK
    /* @KEEP_COMMENT: 执行栈保护 */
    ttosStackProtect(taskCB->taskControlId);
#endif /* CONFIG_PROTECT_STACK */

    (void)ttosTaskStateChanged(taskCB, TRUE);
    (void)ttosEnableTaskDispatchWithLock();
}

/*
 * @brief:
 *    设置周期任务等待队列和截止期队列通知时间。
 * @return:
 *    无
 * @tracedREQ:
 * @implements: RTE_DTASK.4.2
 */
void ttosSetPWaitAndExpireTick(void)
{
    T_TTOS_PeriodTaskNode *node;
    /*
     * @KEEP_COMMENT: 调用list_first(DT.8.9)获取周期任务等待队列
     * ttosManager.periodWaitedQueue中第一个任务保存至节点node
     */
    node = (T_TTOS_PeriodTaskNode *)list_first(&ttosManager.periodWaitedQueue);

    /* @REPLACE_BRACKET: node不为NULL */
    if (node != NULL)
    {
        /* @KEEP_COMMENT: 调用tbspSetPWaitTick(DT.6.9)设置周期等待队列的通知时间
         */
        (void)tbspSetPWaitTick(node->waitedTime);
    }

    /*
     * @KEEP_COMMENT: 调用list_first(DT.8.9)获取截止期等待队列
     * ttosManager.expireWaitedQueue中第一个任务保存至节点node
     */
    node = (T_TTOS_PeriodTaskNode *)list_first(&ttosManager.expireWaitedQueue);

    /* @REPLACE_BRACKET: node不为NULL */
    if (node != NULL)
    {
        /* @KEEP_COMMENT:
         * 调用tbspSetPExpireTick(DT.6.10)设置截止期等待队列的通知时间 */
        (void)tbspSetPExpireTick(node->waitedTime);
    }
}

/**
 * @brief
 *       创建任务。
 * @param[in]: taskParam: 待创建的任务参数
 * @param[out]: taskID: 创建的任务标识符
 * @return
 *       TTOS_CALLED_FROM_ISR: 从中断中调用。
 *       TTOS_INVALID_ADDRESS: taskParam或taskID或任务入口地址、或栈地址为NULL。
 *       TTOS_INVAILD_VERSION:版本不一致。
 *       TTOS_INVALID_NUMBER: 任务优先级不是0-31。
 *       TTOS_INVALID_SIZE: 栈大小小于4KB或周期任务的执行时间大于周期时间。
 *       TTOS_TOO_MANY: 系统中正在使用的任务数已达到用户配置的最大任务数(静态
 *                                  配置的任务数+ 可创建的任务数)。
 *       TTOS_UNSATISFIED: 分配任务对象失败。
 *       TTOS_INVALID_NAME:
 * 对象名name字符串中存在ASCII取值范围不在[32,126]之间的字符
 *                                    或name为空串或者空指针。
 *       TTOS_OK: 成功创建任务。
 * @implements: RTE_DTASK.4.3
 */
T_TTOS_ReturnCode TTOS_CreateTask(T_TTOS_ConfigTask *taskParam, TASK_ID *taskID)
{
    T_TTOS_TaskControlBlock *taskCB = NULL;
    T_BOOL status;
    T_UBYTE *lTaskStack;
    T_ULONG tptr, tstackbase;
    T_TTOS_ReturnCode ret;
    TBSP_MSR_TYPE msr = 0U;

    /* @REPLACE_BRACKET: TRUE == ttosIsISR() */
    if (TRUE == ttosIsISR())
    {
        /* @REPLACE_BRACKET: TTOS_CALLED_FROM_ISR */
        return (TTOS_CALLED_FROM_ISR);
    }

    /* @REPLACE_BRACKET: NULL == taskParam */
    if (NULL == taskParam)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ADDRESS */
        return (TTOS_INVALID_ADDRESS);
    }

    /* @REPLACE_BRACKET: 版本是否一致 */
    if (FALSE ==
        ttosCompareVerison(taskParam->objVersion, (T_UBYTE *)TTOS_OBJ_CONFIG_CURRENT_VERSION))
    {
        /* @REPLACE_BRACKET: TTOS_INVAILD_VERSION */
        return (TTOS_INVAILD_VERSION);
    }

    /* @REPLACE_BRACKET: NULL == taskID */
    if (NULL == taskID)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ADDRESS */
        return (TTOS_INVALID_ADDRESS);
    }

    /*判断名字字符串是否符合命名规范*/
    status = ttosVerifyName(taskParam->cfgTaskName, TTOS_OBJECT_NAME_LENGTH);

    /* @REPLACE_BRACKET: FALSE == status */
    if (FALSE == status)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_NAME */
        return (TTOS_INVALID_NAME);
    }

#ifdef TTOS_32_PRIORTIY

    /* @REPLACE_BRACKET: 任务i的优先级超出支持范围 */
    if (taskParam->taskPriority > (TTOS_PRIORITY_NUMBER - 1))
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_NUMBER */
        return (TTOS_INVALID_NUMBER);
    }

#endif

    /* @REPLACE_BRACKET: 任务i的入口地址为空 */
    if (NULL == taskParam->entry)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ADDRESS */
        return (TTOS_INVALID_ADDRESS);
    }

    /* @REPLACE_BRACKET: 任务i的栈小于最小值 */
    if (taskParam->stackSize < TTOS_MINIMUM_STACK_SIZE)
    {
        KLOG_E("Stack too small: min size 0x%lx stack size 0x%x", TTOS_MINIMUM_STACK_SIZE,
               taskParam->stackSize);
        /* @REPLACE_BRACKET: TTOS_INVALID_SIZE */
        return (TTOS_INVALID_SIZE);
    }

    /* @REPLACE_BRACKET: 周期任务i的持续时间大于周期时间 */
    if ((TTOS_SCHED_PERIOD == taskParam->taskType) &&
        (taskParam->durationTime > taskParam->periodTime))
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_SIZE */
        return (TTOS_INVALID_SIZE);
    }

    /* @REPLACE_BRACKET: 任务的栈指针为空 */
    if (NULL == taskParam->taskStack)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ADDRESS */
        return (TTOS_INVALID_ADDRESS);
    }

    /* @REPLACE_BRACKET: 任务的栈未对齐 */
    if (!is_aligned((uintptr_t)taskParam->taskStack, DEFAULT_TASK_STACK_ALIGN_SIZE))
    {
        KLOG_E("Stack not aligned: align size 0x%lx stack %p", DEFAULT_TASK_STACK_ALIGN_SIZE,
               taskParam->taskStack);
        /* @REPLACE_BRACKET: TTOS_INVALID_ALIGNED */
        return (TTOS_INVALID_ALIGNED);
    }

    TBSP_GLOBALINT_DISABLE_WITH_LOCK(msr);
    ret = ttosAllocateObject(TTOS_OBJECT_TASK, (T_VOID **)&taskCB);

    /* @REPLACE_BRACKET: TTOS_OK != ret */
    if (TTOS_OK != ret)
    {
        TBSP_GLOBALINT_ENABLE_WITH_LOCK(msr);
        /* @REPLACE_BRACKET: ret */
        return (ret);
    }

    TBSP_GLOBALINT_ENABLE_WITH_LOCK(msr);

    /* @REPLACE_BRACKET: 如果用户配置为对栈填充 */
    if (TRUE == ttosTaskStackFilledFlag)
    {
        lTaskStack = taskParam->taskStack;
        /* @KEEP_COMMENT: 对需要用到的栈进行数据初始化 */
        tptr = (T_ULONG)lTaskStack;
        tstackbase = (T_ULONG)lTaskStack + taskParam->stackSize;

        /* @REPLACE_BRACKET: <tptr>指向的地址小于栈的基地址 */
        while (tptr < tstackbase)
        {
            /* @KEEP_COMMENT: 将<tptr>指向的地址赋值为0xab并进行自加 */
            *(T_UBYTE *)tptr = 0xabU;
            tptr++;
        }
    }

    ttosInitTask(taskCB, taskParam, taskID);
    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}
