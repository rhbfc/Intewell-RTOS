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

/* @<MOD_INFO     */
/*
 * @file: DC_RT_2.pdl
 * @brief:
 *    <li>本模块对任务的各种状态和调度进行管理。</li>
 * @implements: DT.2
 */
/* @MOD_INFO> */

/************************头 文 件******************************/

/* @<MOD_HEAD */
#include <ttosBase.h>
#include <ttosInterHal.h>
#include <ttosInterTask.inl>
#include <ttosUtils.inl>

#define KLOG_LEVEL KLOG_WARN
#define KLOG_TAG "Kernel"
#include <klog.h>

#include <trace.h>

#undef KLOG_D
#define KLOG_D(...)

/* @MOD_HEAD> */

/************************宏 定 义******************************/

/************************类型定义******************************/
T_VOID osFaultHook(T_TTOS_HMType type, T_VOID *arg1, T_WORD arg2, T_WORD errorCode);
T_VOID sysStackSet(T_VOID *sysStack);

/************************外部声明******************************/

/* @<MOD_EXTERN */

/* 处理任务删除自身的任务ID */
T_EXTERN TASK_ID ttosDeleteHandlerTaskID;

T_EXTERN T_VOID ttosDispatchSignal(T_TTOS_TaskControlBlock *taskCB);

T_EXTERN T_BOOL pthreadIsCancelableForAsync(T_TTOS_TaskControlBlock *taskCB);

T_EXTERN T_VOID TTOS_SwitchTaskHook(TASK_ID oldTask, TASK_ID newTask);
T_EXTERN void ttosDisableTaskDispatchWithLock(void);
T_EXTERN void fpscrInit(void);

/* @MOD_EXTERN> */

/************************前向声明******************************/
/************************模块变量******************************/
T_MODULE void ttosTaskSchedule(T_UWORD cpuID, TBSP_MSR_TYPE msr);

/************************全局变量******************************/

/* @<MOD_VAR */
#if defined(CONFIG_SMP)
#if CONFIG_SMP == 1
/* 调度开关 */
T_ULONG ttosDisableScheduleLevel[CONFIG_MAX_CPUS];

/*绑定任务优先级就绪队列对应的位图*/
T_TTOS_PriorityMap ttosPriorityBitMap[CONFIG_MAX_CPUS];

/*非绑定任务优先级就绪队列对应的位图*/
T_TTOS_PriorityMap ttosGlobalPriorityBitMap;

#else /*CONFIG_SMP*/
/* 调度开关 */
T_ULONG ttosDisableScheduleLevel;

#ifdef TTOS_32_PRIORTIY
/* 优先级位图 */
T_UWORD ttosPriorityBitMap;
#else
T_TTOS_PriorityMap ttosPriorityBitMap;
#endif /*TTOS_32_PRIORTIY*/
#endif /*CONFIG_SMP*/
#else  /*defined(CONFIG_SMP)*/
/* 调度开关 */
T_ULONG ttosDisableScheduleLevel;

#ifdef TTOS_32_PRIORTIY
/* 优先级位图 */
T_UWORD ttosPriorityBitMap;
#else
T_TTOS_PriorityMap ttosPriorityBitMap;
#endif /*TTOS_32_PRIORTIY*/
#endif /*defined(CONFIG_SMP)*/

/* @MOD_VAR> */

/************************实   现*******************************/

/* @MODULE> */

__attribute__((weak)) void syscall_stack_set(void *stack) {}
void __weak arch_switch_to_kernel_mmu(void){};

/*
 * @brief:
 *    任务首次运行时入口。
 * @return:
 *    无
 * @implements: RTE_DTASK.31.1
 */
T_MODULE void tEntryHandler(void)
{
    T_TTOS_TaskControlBlock *task;
    T_UWORD cpuID = cpuid_get();

    /* 在第一次进入任务前进行解锁 */
    TTOS_KERNEL_UNLOCK();
    /* @KEEP_COMMENT: 获取当前运行任务ttosManager.runningTask存放至变量task */
    task = ttosGetRunningTaskWithCpuID(cpuID);
    /* @KEEP_COMMENT: 根据ttosManager.runningTask->id调用TTOS_EnterTaskHook() */
    (void)TTOS_EnterTaskHook(task->taskControlId);
    /* @KEEP_COMMENT: 设置调度开关ttosDisableScheduleLevel为零 */
    ttosSetDisableScheduleLevel(cpuID, 0U);
    /* @KEEP_COMMENT: 调用ttosSchedule(DT.2.20)进行任务调度 */
    (void)ttosSchedule();
#ifdef _HARD_FLOAT_
    /* @KEEP_COMMENT: 调用tbspInitFPSCR(DT.6.25)初始化FPSCR寄存器 */
    fpscrInit();
#endif

    syscall_stack_set(task->kernelStackTop);

    /* @KEEP_COMMENT: 调用vbspEnableGlobalInt(DVB.1.5)使能虚拟中断 */
    TBSP_GLOBALINT_OPEN();

    task->wait.returnArgument = 0;

    /* @KEEP_COMMENT: 根据ttosManager.runningTask->id调用TTOS_TaskStartHook() */
    TTOS_TaskStartHook(task->taskControlId);

    /* @KEEP_COMMENT: 调用ttosManager.runningTask->entry */
    (task->entry)(task->arg);

    /* @KEEP_COMMENT: 禁止调度 */
    (void)ttosDisableTaskDispatchWithLock();
    (void)ttosTaskExit(task, task->wait.returnArgument);
    /* @KEEP_COMMENT: 任务退出时，没有成功退出
     * 。有可能是开关调度没有配对使用造成的。*/
    (void)osFaultHook(TTOS_HM_RUNNING_CHECK_EXCEPTION, (T_VOID *)__FUNCTION__, __LINE__,
                      TTOS_TASK_ENTRY_RETURN_ERROR);
}

/*
 * @brief:
 *    获取指定CPU上的候选任务。
 * @param[in]: cpuID: CPU ID，仅仅多核才使用此参数
 * @return:
 *    指定CPU上的候选任务。
 * @implements: RTE_DTASK.31.2
 */
T_TTOS_TaskControlBlock *ttosGetCandidateTask(T_UWORD cpuID)
{
    T_TTOS_TaskControlBlock *candidateTask = NULL;
#if defined(CONFIG_SMP)
    T_TTOS_TaskControlBlock *globalReadyTask = NULL;
    T_TTOS_TaskControlBlock *runningTask = NULL;
    T_TTOS_TaskControlBlock *globalRunningTask = NULL;
    T_UWORD state = (T_UWORD)TTOS_TASK_READY;

    /* @KEEP_COMMENT: 候选任务进入 */
    TTOS_CandidateEnterHook();

    /* @REPLACE_BRACKET: U(0) == (CPUSET_ISSET(TTOS_CPUSET_ENABLED(),cpuID)) */
    if (U(0) == (CPU_ISSET(cpuID, TTOS_CPUSET_ENABLED())))
    {
        /* @KEEP_COMMENT: 指定的CPU下线时会执行此分支。
         *CPU下线时，会清除下线CPU的使能位，然后向下线的CPU发送重调度IPI。下线CPU在
         *中断退出时，会调用ttosSchedule切换回IDLE任务，这样非绑定的任务可以重新调度。
         */
        candidateTask = ttosManager.idleTask[cpuID];
        KLOG_D("candidateTask:%p, name:%s", candidateTask, candidateTask->objCore.objName);
        /* @KEEP_COMMENT: 候选任务退出 */
        TTOS_CandidateLeaveHook();
        return (candidateTask);
    }

    /* @REPLACE_BRACKET: U(0) != CPUSET_ISSET(TTOS_CPUSET_RESERVED(),cpuID) */
    if (U(0) != CPU_ISSET(cpuID, TTOS_CPUSET_RESERVED()))
    {
        /* @KEEP_COMMENT: 预留的CPU只能运行绑定到此CPU上的任务*/
        candidateTask = ttosManager.priorityQueueTask[cpuID];
    }

    else
    {
        /* @KEEP_COMMENT: 获取最高优先级的非绑定就绪任务*/
        globalReadyTask = ttosManager.globalTask;
        runningTask = ttosGetRunningTaskWithCpuID(cpuID);
        /* @REPLACE_BRACKET: (CPU_NONE == runningTask->smpInfo.affinityCpuIndex)
         * && (FALSE == runningTask->smpInfo.isChangedToPrivate) */
        if ((CPU_NONE == runningTask->smpInfo.affinityCpuIndex) &&
            (FALSE == runningTask->smpInfo.isChangedToPrivate))
        {
            /*
             *运行任务为非绑定运行，非绑定运行任务不在就绪队列中，需要单独考虑。
             *非绑定的运行任务由全局任务改为绑定时，在任务切换时通过执行ttosGTaskRQueuePut()
             *重新放入绑定就绪队列，在运行到切回任务时，如果全局任务绑定的CPU为当前CPU，
             *会通过执行ttosGetCandidateTask()再次参与调度。
             */
            globalRunningTask = runningTask;

            /* @REPLACE_BRACKET: TRUE == runningTask->smpInfo.isRotated */
            if (TRUE == runningTask->smpInfo.isRotated)
            {
                struct list_head *chain;
                /* @KEEP_COMMENT: 获取task任务优先级对应的就绪队列 */
                chain = ttosReadyQueueGet(runningTask->smpInfo.affinityCpuIndex,
                                          runningTask->taskCurPriority);

                /* @REPLACE_BRACKET: FALSE == list_is_empty(chain) */
                if (FALSE == list_is_empty(chain))
                {
                    /* @KEEP_COMMENT:
                     * task任务所在的任务优先级就绪队列还有未运行的任务
                     * ，需要运行此队列上的第一个任务。*/
                    globalRunningTask = NULL;
                }
            }
        }

        /* @KEEP_COMMENT:
         * 获取最高优先级的绑定就绪任务，由于有IDLE任务，candidateTask一定不为NULL。*/
        candidateTask = ttosManager.priorityQueueTask[cpuID];

        /* @KEEP_COMMENT: 非预留的CPU可运行绑定到此CPU上的任务和非绑定的任务*/
        /* @REPLACE_BRACKET: NULL != globalReadyTask */
        if (NULL != globalReadyTask)
        {
            /* @REPLACE_BRACKET: globalReadyTask->taskCurPriority <
             * candidateTask->taskCurPriority */
            if (globalReadyTask->taskCurPriority < candidateTask->taskCurPriority)
            {
                /* @KEEP_COMMENT:
                 * 非绑定就绪任务的优先级大于绑定就绪任务的优先级时，则候选任务为非绑定任务。*/
                candidateTask = globalReadyTask;
            }
        }

        /* @REPLACE_BRACKET: NULL != globalRunningTask */
        if (NULL != globalRunningTask)
        {
            /* @KEEP_COMMENT:
             * 运行任务为非绑定运行，非绑定运行任务不在就绪队列中，需要单独考虑。*/
            if ((state == (globalRunningTask->state & state)) &&
                (globalRunningTask->taskCurPriority <= candidateTask->taskCurPriority))
            {
                /*
                 *非绑定运行任务的优先级大于等于就绪任务的优先级时，则候选任务为非运行任务。
                 *
                 *为了减少系统开销，只有就绪任务的优先级大于运行任务的优先级时，则候选任务才
                 *为就绪任务。
                 */
                candidateTask = globalRunningTask;
            }
        }
    }
#else  /*defined(CONFIG_SMP)*/
    candidateTask = ttosManager.priorityQueueTask;
#endif /*defined(CONFIG_SMP)*/

    /* @KEEP_COMMENT: 候选任务退出 */
    TTOS_CandidateLeaveHook();

    /* @REPLACE_BRACKET: candidateTask */
    return (candidateTask);
}

/*
 * @brief:
 *    进行任务调度。
 * @return:
 *    无
 * @implements: RTE_DTASK.31.3
 */
void ttosSchedule(void)
{
    T_UWORD cpuID;
    TBSP_MSR_TYPE msr = 0U;
    /* @KEEP_COMMENT: 禁止虚拟中断 */
    TBSP_GLOBALINT_DISABLE(msr);
    cpuID = cpuid_get();

    /* @REPLACE_BRACKET: 调度开关ttosDisableScheduleLevel不等于零 */
    if (ttosGetDisableScheduleLevelWithCpuID(cpuID) != 0U)
    {
        TBSP_GLOBALINT_ENABLE(msr);
        return;
    }

    TTOS_KERNEL_LOCK();
    (void)ttosTaskSchedule(cpuID, msr);
}

/*
 * @brief:
 *    指定CPU上的进行任务调度。
 * @param[in]: cpuID: CPU ID，仅仅多核才使用此参数
 * @param[in]: msr, CPU状态字相关定义
 * @return:
 *    无
 * @implements: RTE_DTASK.31.4
 */
T_MODULE void ttosTaskSchedule(T_UWORD cpuID, TBSP_MSR_TYPE msr)
{
    T_TTOS_TaskControlBlock *newTask, *oldTask;
    T_UWORD dormantState = (T_UWORD)TTOS_TASK_DORMANT;
    T_UWORD nonRunState = (T_UWORD)TTOS_TASK_NONRUNNING_STATE;

    /* 调度器进入 */
    TTOS_ScheduleEnterHook();

    /* @KEEP_COMMENT: 获取当前运行任务ttosManager.runningTask存放至变量oldTask
     */
    oldTask = ttosGetRunningTaskWithCpuID(cpuID);
    /* @KEEP_COMMENT:
     * 获取最高优先级任务ttosManager.priorityQueueTask存放至变量newTask */
    newTask = ttosGetCandidateTask(cpuID);

    /*
        提前保存开关中断状态,删除后续的开中断动作。
        若不删除后徐的开中断动作，则可能出现:
        cpu0                                    cpu1
        TTOS_KERNEL_LOCK                        syscall
        ttosTaskSchedule                        xx_driver_lock
        ttosTaskSchedule中产生中断               TTOS_ReleaseSema
        中断中,驱动模块xx_driver_lock             TTOS_KERNEL_LOCK

        造成死锁
    */

    arch_context_save_cpu_state(oldTask, msr);

    /* @REPLACE_BRACKET: newTask != oldTask */
    while (newTask != oldTask)
    {

        // KLOG_D ("newTask:%p, oldTask:%p", newTask, oldTask);
        /* @KEEP_COMMENT: 设置调度开关ttosDisableScheduleLevel为1 */
        ttosSetDisableScheduleLevel(cpuID, 1U);

        /*
         * @REPLACE_BRACKET: oldTask任务状态不为TTOS_TASK_NONRUNNING_STATE &&
         * oldTask任务不允许被抢占
         */
        if ((0U == (oldTask->state & nonRunState)) && ((oldTask->preemptCount > 0U)))
        {
            break;
        }

        trace_switch_task(oldTask, newTask);

        /* @REPLACE_BRACKET: 调用任务切换钩子函数列表 */
        (void)TTOS_SwitchTaskHook(oldTask->taskControlId, newTask->taskControlId);
        /* @KEEP_COMMENT: 根据oldTask任务的ID号调用TTOS_LeaveTaskHook() */
        (void)TTOS_LeaveTaskHook(oldTask->taskControlId);
        /* @KEEP_COMMENT: 设置newTask任务为当前运行任务ttosManager.runningTask
         */
        ttosSetRunningTaskWithCpuID(cpuID, newTask);
        oldTask->smpInfo.cpuIndex = CPU_NONE;

        /* @KEEP_COMMENT: 记录newTask任务的核心态任务栈 */
        (void)sysStackSet(newTask->kernelStackTop);

#if defined(CONFIG_SMP)
        /*
         *全局任务放回就绪队列与全局任务被设置为非当前运行任务必须是原子性，否
         *则在如下使能中断后，全局任务如果被唤醒放入就绪队列后，再执行此代码会
         *存在全局任务被多次插入链表，整个就绪链表会乱，系统不能正常运行。
         *
         */
        ttosGTaskRQueuePut(oldTask);
        /* 全局任务作为运行任务时，需要从全局就绪队列中摘除。*/
        ttosGTaskRQueueRemove(newTask);
#endif
        /* @KEEP_COMMENT: 使能虚拟中断
         * ，这样任务上下文保存的中断开关才是正确的。*/
        // TBSP_GLOBALINT_ENABLE (msr);

        /* @REPLACE_BRACKET: oldTask任务状态不是TTOS_TASK_DORMANT */
        if (dormantState != (oldTask->state & dormantState))
        {
            /* @REPLACE_BRACKET: 保存oldTask任务上下文 */
            if (1U == tbspSaveTaskContext(&oldTask->switchContext, cpuID))
            {
                /*任务恢复回来后会运行到此处，栈地址已经改变，需要重新获取cpuID。*/
                cpuID = cpuid_get();
                /* @KEEP_COMMENT:
                 * 获取当前运行任务ttosManager.runningTask存放至变量oldTask */
                oldTask = ttosGetRunningTaskWithCpuID(cpuID);
                /* @KEEP_COMMENT:
                 * ttosManager.runningTask->id作为参数调用TTOS_EnterTaskHook()
                 */
                (void)TTOS_EnterTaskHook(oldTask->taskControlId);
                /* @KEEP_COMMENT: 禁止虚拟中断 */
                TBSP_GLOBALINT_DISABLE(msr);
                /*
                 * @KEEP_COMMENT:
                 * 获取最高优先级任务ttosManager.priorityQueueTask存放至
                 * 变量newTask
                 */
                newTask = ttosGetCandidateTask(cpuID);
                continue;
            }
        }

        /* @REPLACE_BRACKET: newTask任务的状态为TTOS_TASK_FIRSTRUN */
        if (TTOS_TASK_FIRSTRUN == (newTask->state & TTOS_TASK_FIRSTRUN))
        {
            /* @KEEP_COMMENT: 清除newTask任务TTOS_TASK_FIRSTRUN标记 */
            newTask->state &= (~TTOS_TASK_FIRSTRUN);
            /* @KEEP_COMMENT: 加载newTask任务的任务栈 */
            (void)tbspLoadStack(newTask->kernelStackTop);

            /* 调度器结束 */
            TTOS_ScheduleLeaveHook();

            arch_switch_to_kernel_mmu();

            /* @KEEP_COMMENT: 调用tEntryHandler(DT.2.19)运行newTask任务 */
            tEntryHandler();
        }
        else
        {
            /* @KEEP_COMMENT: 恢复newTask任务的上下文 */
            (void)tbspRestoreTaskContext(&newTask->switchContext, cpuID);
        }
    }

    /* @REPLACE_BRACKET: newTask任务的状态为TTOS_TASK_FIRSTRUN */
    if ((newTask == oldTask) && TTOS_TASK_FIRSTRUN == (newTask->state & TTOS_TASK_FIRSTRUN))
    {
        /* 任务重启后会执行此分支。*/
        /* @KEEP_COMMENT: 清除newTask任务的TTOS_TASK_FIRSTRUN状态 */
        newTask->state &= (~TTOS_TASK_FIRSTRUN);
        /* @KEEP_COMMENT: 加载newTask任务的核心态任务栈 */
        (void)tbspLoadStack(newTask->kernelStackTop);

        /* 调度器结束 */
        TTOS_ScheduleLeaveHook();

        /* @KEEP_COMMENT: 调用tEntryHandler(DT.2.19)运行newTask任务 */
        tEntryHandler();
    }

    /* 在调度退出时进行解锁 */
    TTOS_KERNEL_UNLOCK();
    /* @KEEP_COMMENT: 设置调度开关ttosDisableScheduleLevel为0 */
    ttosSetDisableScheduleLevel(cpuID, 0U);
    /* @KEEP_COMMENT: 使能虚拟中断 */

    arch_context_restore_cpu_state(oldTask);

    /* 调度器结束 */
    TTOS_ScheduleLeaveHook();
}

/*
 * @brief:
 *    进行任务调度。
 * @return:
 *    无
 * @notes:
 *    本接口在获取内核锁并且是在可重调度的条件下使用。
 * @implements: RTE_DTASK.31.5
 */
void ttosScheduleInKernelLock(void)
{
    T_UWORD cpuID;
    TBSP_MSR_TYPE msr = 0U;
    /* @KEEP_COMMENT: 进入此接口时，是获取内核锁的，并且是可以重调度的。*/
    TBSP_GLOBALINT_DISABLE(msr);
    cpuID = cpuid_get();
    /* @KEEP_COMMENT:
     * ttosTaskSchedule()中会重置调度开关，所以此处不需要显示的使能调度开关。*/
    ttosTaskSchedule(cpuID, msr);
}

/*
 * @brief:
 *    进行任务调度。
 * @return:
 *    无
 * @notes:
 *    本接口在禁止中断并且获取内核锁的条件下使用。
 * @implements: RTE_DTASK.31.6
 */
void ttosScheduleInIntDisAndKernelLock(TBSP_MSR_TYPE msr)
{
    T_UWORD cpuID;
    cpuID = cpuid_get();

    /* @REPLACE_BRACKET: 调度开关ttosDisableScheduleLevel不等于零 */
    if (ttosGetDisableScheduleLevelWithCpuID(cpuID) != 0U)
    {
        TTOS_KERNEL_UNLOCK();
        TBSP_GLOBALINT_ENABLE(msr);
        return;
    }

    ttosTaskSchedule(cpuID, msr);
}

/*
 * @brief:
 *    判断指定CPU是否需要重新调度。
 * @param[in]:cpuID:影响的cpuID
 * @return:
 *    FALSE:不需要重新调度。
 *    TRUE:需要重新调度。
 * @implements: RTE_DTASK.31.7
 */
T_BOOL ttosIsNeedRescheduleWithCpuId(T_UWORD cpuID)
{
    T_TTOS_TaskControlBlock *newTask;
    T_TTOS_TaskControlBlock *oldTask;
    T_UWORD state = (T_UWORD)TTOS_TASK_NONRUNNING_STATE;

    /*
     *1.此接口都是对全局变量进行读访问，所有没有使用自旋锁。
     *2.即使获取的oldTask、newTask有变化，最多会多发一次重调度IPI，
     *并不会影响任务调度的实时性。
     */
    /* @REPLACE_BRACKET: ttosGetDisableScheduleLevelWithCpuID(cpuID) != U(0) */
    if (ttosGetDisableScheduleLevelWithCpuID(cpuID) != 0U)
    {
        /* @REPLACE_BRACKET: FALSE */
        return (FALSE);
    }

    oldTask = ttosGetRunningTaskWithCpuID(cpuID);

    /* @REPLACE_BRACKET:  oldTask没有主动放弃CPU并且oldTask任务不允许被抢占*/
    if ((0U == (oldTask->state & state)) && ((oldTask->preemptCount > 0U)))
    {
        /* 指定CPU的运行任务没有主动放弃CPU并且是不允许被抢占的。*/
        /* @REPLACE_BRACKET: FALSE */
        return (FALSE);
    }

    newTask = ttosGetCandidateTask(cpuID);

    /* @REPLACE_BRACKET: newTask == oldTask */
    if (newTask == oldTask)
    {
        /* @REPLACE_BRACKET: FALSE */
        return (FALSE);
    }

    /* @REPLACE_BRACKET: TRUE */
    return (TRUE);
}
