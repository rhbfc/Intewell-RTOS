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
#include <ttosHandle.h>
#include <ttosInterTask.inl>

/* @MOD_HEAD> */

/************************宏 定 义******************************/
/************************类型定义******************************/
/************************外部声明******************************/
T_VOID osFaultHook(T_TTOS_HMType type, T_VOID *arg1, T_WORD arg2, T_WORD errorCode);
INT32 ipiRescheduleEmit(cpu_set_t *cpus, T_BOOL selfexcluded);

/************************前向声明******************************/
void ttosDisableTaskDispatchWithLock(void);
/************************模块变量******************************/
// T_MODULE T_WORD errnoNoTask = 0;

static T_BOOL ttosReadyQueueContainsTask(struct list_head *chain, T_TTOS_TaskControlBlock *task)
{
    struct list_node *node;

    if ((NULL == chain) || (NULL == task))
    {
        return FALSE;
    }

    node = list_first(chain);
    while (node != NULL)
    {
        if (node == &(task->objCore.objectNode))
        {
            return TRUE;
        }
        node = list_next(node, chain);
    }

    return FALSE;
}

/************************全局变量******************************/

/************************实   现*******************************/
/*
 * @brief:
 *    禁止任务调度，可嵌套使用。
 * @return:
 *    无
 * @tracedREQ:
 * @implements: RTE_DTASK.40.8
 */
void ttosDisableTaskDispatch(void)
{
    /* @KEEP_COMMENT: 禁止任务调度 */
    ttosIncDisableScheduleLevel();
}

/*
 * @brief:
 *     使能任务调度，可嵌套使用。
 * @return:
 *    无
 * @tracedREQ:
 * @implements: RTE_DTASK.40.9
 */
void ttosEnableTaskDispatch(void)
{
    /*正常情况下执行到此处时，调度是禁止了的。*/
    T_UWORD cpuID = cpuid_get();

    /* @REPLACE_BRACKET:ttosGetDisableScheduleLevelWithCpuID(cpuID) != U(0) */
    if (ttosGetDisableScheduleLevelWithCpuID(cpuID) != 0U)
    {
        /* @REPLACE_BRACKET: (ttosDecAndGetDisableScheduleLevel(cpuID)) ==
         * U(0)*/
        if ((ttosDecAndGetDisableScheduleLevel(cpuID)) == 0U)
        {
            /* @KEEP_COMMENT: 调用ttosSchedule(DT.2.20)进行任务调度 */
            (void)ttosSchedule();
        }
    }

    else
    {
        /*正常情况下不应该执行此分分支。*/
        (void)osFaultHook(TTOS_HM_RUNNING_CHECK_EXCEPTION, (T_VOID *)__FUNCTION__, __LINE__,
                          (T_TTOS_ErrorCode)cpuID);
    }
}

/*
 * @brief:
 *    禁止任务调度，可嵌套使用。
 * @return:
 *    无
 * @tracedREQ:
 * @implements: RTE_DTASK.40.10
 */
void ttosDisableTaskDispatchWithLock(void)
{
    /* @KEEP_COMMENT: 禁止任务调度并获取自旋锁 */
    ttosDisableTaskDispatch();
    TTOS_KERNEL_LOCK();
}

/*
 * @brief:
 *     使能任务调度，可嵌套使用。
 * @return:
 *    无
 * @tracedREQ:
 * @implements: RTE_DTASK.40.11
 */
void ttosEnableTaskDispatchWithLock(void)
{
    /*正常情况下执行到此处时，调度是禁止了的。*/
    T_UWORD cpuID = cpuid_get();

    /* @REPLACE_BRACKET: ttosGetDisableScheduleLevelWithCpuID(cpuID) == U(1)*/
    if (ttosGetDisableScheduleLevelWithCpuID(cpuID) == 1U)
    {
        /* @KEEP_COMMENT:  可进行任务重调度*/
        (void)ttosScheduleInKernelLock();
    }

    /* @REPLACE_BRACKET: ttosGetDisableScheduleLevelWithCpuID(cpuID) > U(1)*/
    else if (ttosGetDisableScheduleLevelWithCpuID(cpuID) > 1U)
    {
        /* @KEEP_COMMENT:  不能进行任务重调度*/
        TTOS_KERNEL_UNLOCK();
        ttosDecDisableScheduleLevel(cpuID);
    }

    else
    {
        /*正常情况下不应该执行此分分支。*/
        (void)osFaultHook(TTOS_HM_RUNNING_CHECK_EXCEPTION, (T_VOID *)__FUNCTION__, __LINE__,
                          (T_TTOS_ErrorCode)cpuID);
    }
}

/*
 * @brief
 *       改变任务的优先级。
 * @param[in]      theTask: 待改变的任务控制块
 * @param[in]  newPriority: 新的任务优先级
 * @param[in]    prependIt: 任务是否优先考虑
 * @return
 *     无
 * @implements: RTE_DTASK.40.12
 */
void taskChangePriority(T_TTOS_TaskControlBlock *theTask, T_UBYTE newPriority, T_BOOL prependIt)
{
    T_TTOS_Task_Queue_Control *queue = NULL;
    struct list_head *readyQueue = NULL;
    TBSP_MSR_TYPE msr = 0U;
    T_UBYTE beforePriority = theTask->taskCurPriority;
    T_UWORD state = (T_UWORD)TTOS_TASK_NONRUNNING_STATE;
    T_BOOL inReadyQueue = FALSE;
    T_BOOL nodeLinked = FALSE;
    /*此接口仅仅使用于任务优先级改变时，并没有在优先级等待队列上。*/
    /*定时器中断处理程序中有可能要操作就绪队列，所以此处需要关中断。*/
    TBSP_GLOBALINT_DISABLE(msr);
    /* @KEEP_COMMENT: 设置当前使用优先级*/
    theTask->taskCurPriority = newPriority;

    /* @REPLACE_BRACKET: <task>任务状态不是TTOS_TASK_NONRUNNING_STATE */
    if (0 == (theTask->state & state))
    {
#if defined(CONFIG_SMP)
        /* 非绑定的运行任务是从就绪队列摘除了的*/
        /* @REPLACE_BRACKET: (CPU_NONE != theTask->smpInfo.affinityCpuIndex) ||
         * (CPU_NONE == theTask->smpInfo.cpuIndex)*/
        if ((CPU_NONE != theTask->smpInfo.affinityCpuIndex) ||
            (CPU_NONE == theTask->smpInfo.cpuIndex))
        {
#endif
            readyQueue = ttosReadyQueueGet(theTask->smpInfo.affinityCpuIndex, beforePriority);
            inReadyQueue = ((theTask->objCore.objectNode.next != NULL) &&
                            (theTask->objCore.objectNode.prev != NULL) &&
                            (TRUE == ttosReadyQueueContainsTask(readyQueue, theTask)));
            nodeLinked = (theTask->objCore.objectNode.next != NULL);

            /* 绑定的就绪任务或者非绑定非运行任务是才需要从就绪队列摘除*/
            /* @KEEP_COMMENT: 将任务从以前的优先级链表中移除 */
            if (TRUE == inReadyQueue)
            {
                list_delete(&(theTask->objCore.objectNode));
            }
            else if (TRUE == nodeLinked)
            {
                /*
                 * Node is linked but not in expected ready queue. Avoid
                 * re-linking into another queue and corrupting chains.
                 */
                ttosSetHighestPriorityTask(theTask->smpInfo.affinityCpuIndex);
                TBSP_GLOBALINT_ENABLE(msr);
                return;
            }
            /* 优化会导致指令重新排序，添加编译屏障保证顺序排列指令 */
            TTOS_COMPILER_MEMORY_BARRIER();

            /* @REPLACE_BRACKET: TRUE ==
             * ttosReadyQueueIsEmpty(theTask->smpInfo.affinityCpuIndex,
             * beforePriority)*/
            if ((TRUE == inReadyQueue) &&
                (TRUE == ttosReadyQueueIsEmpty(theTask->smpInfo.affinityCpuIndex, beforePriority)))
            {
                /* @KEEP_COMMENT:
                 * 清除<task>任务的优先级在优先级位图中ttosPriorityBitMap对应的位
                 */
                TBSP_CLEAR_PRIORITYBITMAP(theTask->smpInfo.affinityCpuIndex, beforePriority);
            }

            /* @KEEP_COMMENT: 修改优先级位图 */
            TBSP_SET_PRIORITYBITMAP(theTask->smpInfo.affinityCpuIndex, newPriority);

            /* @REPLACE_BRACKET: prependIt == FALSE */
            if (prependIt == FALSE)
            {
                /* @KEEP_COMMENT:
                 * 将<task>任务插入对应优先级就绪队列ttosManager.priorityQueue尾部
                 */
                ttosReadyQueuePut(theTask, FALSE);
            }

            else
            {
                /* @KEEP_COMMENT:
                 * 将<task>任务插入对应优先级就绪队列ttosManager.priorityQueue头部
                 */
                ttosReadyQueuePut(theTask, TRUE);
            }

            /* @KEEP_COMMENT: 获取当前优先级最高的任务 */
            ttosSetHighestPriorityTask(theTask->smpInfo.affinityCpuIndex);
#if defined(CONFIG_SMP)
        }

#endif
        (void)ttosTaskStateChanged(theTask, TRUE);
    }
    else if (0 != (theTask->state & TTOS_TASK_BLOCKING_ON_TASK_QUEUE))
    {
        queue = theTask->wait.queue;
        if (queue == NULL)
        {
            TBSP_GLOBALINT_ENABLE(msr);
            return;
        }

        /* @KEEP_COMMENT: 若当前任务在优先级等待队列上阻塞，需要使用任务新优先级重新进行等待队列排序
         */
        if (T_TTOS_QUEUE_DISCIPLINE_PRIORITY == queue->discipline)
        {
            /* @KEEP_COMMENT: 出队*/
            ttosExtractTaskq(queue, theTask);

            /* @KEEP_COMMENT: 入队*/
            ttosEnqueuePriority(queue, theTask);
        }
    }

    /* @KEEP_COMMENT: 恢复中断 */
    TBSP_GLOBALINT_ENABLE(msr);
}

/*
 * @brief:
 *    在就绪队列上对运行任务放进行轮转。
 * @param[out]||[in]: task: 运行任务控制块
 * @return:
 *    无
 * @implements: RTE_DTASK.40.13
 */
void ttosRotateRunningTask(T_TTOS_TaskControlBlock *task)
{
    struct list_head *chain;
    struct list_node *node;
    /* @KEEP_COMMENT: 获取task任务优先级对应的就绪队列 */
    chain = ttosReadyQueueGet(task->smpInfo.affinityCpuIndex, task->taskCurPriority);
#if defined(CONFIG_SMP)
#if CONFIG_SMP == 1
    BOOL ipiEmitFlag = FALSE;

    /* @REPLACE_BRACKET: CPU_NONE == task->smpInfo.affinityCpuIndex*/
    if (CPU_NONE == task->smpInfo.affinityCpuIndex)
    {
        /* @REPLACE_BRACKET:  FALSE == list_is_empty(chain) */
        if (FALSE == list_is_empty(chain))
        {
            /* @KEEP_COMMENT: task任务所在的任务优先级就绪队列还有未运行的任务
             */
            task->smpInfo.isRotated = TRUE;

            /* @REPLACE_BRACKET: FALSE == ttosIsRunningTask(task->taskControlId)
             */
            if (FALSE == ttosIsRunningTask(task->taskControlId))
            {
                ipiEmitFlag = TRUE;
            }
        }
    }

    else
    {
#endif /*CONFIG_SMP*/
#endif
        node = &(task->objCore.objectNode);

        /* @REPLACE_BRACKET: NULL != list_next(node, chain) */
        if (NULL != list_next(node, chain))
        {
            /* task任务所在的任务优先级就绪队列有多个任务 */
            /* @KEEP_COMMENT: 将运行任务从任务优先级就绪队列上移除*/
            list_delete(node);
            /* @KEEP_COMMENT: 将运行任务插入任务优先级就绪队列尾部*/
            list_add_tail(node, chain);
            /* @KEEP_COMMENT: 重新获取最高优先级任务*/
            ttosSetHighestPriorityTask(task->smpInfo.affinityCpuIndex);
#if defined(CONFIG_SMP)
#if CONFIG_SMP == 1

            /* @REPLACE_BRACKET: FALSE == ttosIsRunningTask(task->taskControlId)
             */
            if (FALSE == ttosIsRunningTask(task->taskControlId))
            {
                ipiEmitFlag = TRUE;
            }

#endif /*CONFIG_SMP*/
#endif
        }

#if defined(CONFIG_SMP)
#if CONFIG_SMP == 1
    }

    /* @REPLACE_BRACKET: TRUE == ipiEmitFlag */
    if (TRUE == (unsigned int)ipiEmitFlag)
    {
        /*
         *<task>不是当前CPU上的运行任务，需要发送重调度IPI。
         *由于当前CPU上的运行任务调用此接口时，在调用接口退出时，会进行任务
         *的重调度，所以不需要发送IPI重调度。
         */
        cpu_set_t set;
        CPU_ZERO(&set);
        CPU_SET(task->smpInfo.cpuIndex, &set);
        (T_VOID) ipiRescheduleEmit(&set, TRUE);
    }

#endif /*CONFIG_SMP*/
#endif
}

/**
 * @brief:
 *    获取任务占有互斥信号量的信息。
 *
 *    数据格式是:任务ID，拥有的互斥信号量ID，任务ID，拥有的互斥信号量ID...。
 *
 *    一个任务可以拥有多个互斥信号量。
 *
 * @param[in]: task_occupy: 获取信息的地址
 * @param[in]: max: task_occupy的大小
 * @return:
 *           >=0，表示获取成功，实际获取的数据大小。
 *           -2，表示获取失败，数据超过表的容量。
 * @implements: RTE_DTASK.40.14
 */
int occupy_tbl_get(T_ULONG *task_occupy, int maxAmount)
{
    int listAmount = 0;
    struct list_node *pNode;
    T_TTOS_TaskControlBlock *taskCB;
    T_TTOS_SemaControlBlock *semaCB;
    ttosDisableTaskDispatchWithLock();
    /* @KEEP_COMMENT: 获取表头节点*/
    pNode = list_first(&(ttosManager.inUsedResource[TTOS_OBJECT_SEMA]));

    /* @REPLACE_BRACKET: pNode != NULL*/
    while (pNode != NULL)
    {
        /* @REPLACE_BRACKET: listAmount == max */
        if (listAmount == maxAmount)
        {
            ttosEnableTaskDispatchWithLock();
            /* @REPLACE_BRACKET: -2 */
            return (-2);
        }

        semaCB = (T_TTOS_SemaControlBlock *)(((T_TTOS_ObjResourceNode *)pNode)->objResourceObject);

        /* @KEEP_COMMENT: 现在只考虑互斥信号量的拥有者*/
        /* @REPLACE_BRACKET: TTOS_NULL_OBJECT_ID != semaCB->semHolder */
        if (TTOS_NULL_OBJECT_ID != semaCB->semHolder)
        {
            taskCB = semaCB->semHolder;
            *task_occupy = (T_ULONG)taskCB;
            task_occupy++;
            listAmount++;
            *task_occupy = (T_ULONG)semaCB;
            task_occupy++;
            listAmount++;
        }

        pNode = list_next(pNode, &(ttosManager.inUsedResource[TTOS_OBJECT_SEMA]));
    }

    ttosEnableTaskDispatchWithLock();
    /* @REPLACE_BRACKET: listAmount */
    return (listAmount);
}

/**
 * @brief:
 *    获取任务等待互斥信号量的信息。
 *
 *    数据格式是:任务ID，等待的互斥信号量ID，任务ID，等待的互斥信号量ID...。
 *
 *    一个任务最多等待一个互斥信号量。
 *
 * @param[in]: task_wait: 获取信息的地址
 * @param[in]: max: task_wait的大小
 * @return:
 *           >=0，表示获取成功，实际获取的数据大小。
 *           -2，表示获取失败，数据超过表的容量。
 * @implements: RTE_DTASK.40.15
 */
int wait_tbl_get(unsigned long *task_wait, int maxAmount)
{
    int listAmount = 0;
    struct list_node *pNode;
    T_TTOS_TaskControlBlock *taskCB;
    T_TTOS_SemaControlBlock *semaCB;
    T_UWORD state = (T_UWORD)TTOS_TASK_WAITING_FOR_SEMAPHORE;
    ttosDisableTaskDispatchWithLock();
    /* @KEEP_COMMENT: 获取表头节点*/
    pNode = list_first(&(ttosManager.inUsedResource[TTOS_OBJECT_TASK]));

    /* @REPLACE_BRACKET: pNode != NULL*/
    while (pNode != NULL)
    {
        /* @REPLACE_BRACKET: listAmount == max */
        if (listAmount == maxAmount)
        {
            ttosEnableTaskDispatchWithLock();
            /* @REPLACE_BRACKET: -2 */
            return (-2);
        }

        taskCB = (T_TTOS_TaskControlBlock *)(((T_TTOS_ObjResourceNode *)pNode)->objResourceObject);

        /* @REPLACE_BRACKET: TTOS_TASK_WAITING_FOR_SEMAPHORE ==
         * (TTOS_TASK_WAITING_FOR_SEMAPHORE & taskCB->state) */
        if (state == (state & taskCB->state))
        {
            semaCB = (T_TTOS_SemaControlBlock *)(taskCB->wait.queue->objectCoreID);

            /* @KEEP_COMMENT: 现在只考虑互斥信号量的等待*/
            /* @REPLACE_BRACKET: TTOS_MUTEX_SEMAPHORE == (semaCB->attributeSet &
             * TTOS_SEMAPHORE_MASK) */
            if (TTOS_MUTEX_SEMAPHORE == (semaCB->attributeSet & TTOS_SEMAPHORE_MASK))
            {
                *task_wait = (T_ULONG)taskCB;
                task_wait++;
                listAmount++;
                *task_wait = (T_ULONG)semaCB;
                task_wait++;
                listAmount++;
            }
        }

        pNode = list_next(pNode, &(ttosManager.inUsedResource[TTOS_OBJECT_TASK]));
    }

    ttosEnableTaskDispatchWithLock();
    /* @REPLACE_BRACKET: listAmount */
    return (listAmount);
}

/*
 * @brief:
 *    设置任务的状态。
 * @param[out]||[in]: task: 任务控制块
 * @param[in]: taskState: 需要设置的任务状态
 * @return:
 *    无
 * @implements: RTE_DTASK.40.17
 */
void ttosSetTaskState(T_TTOS_TaskControlBlock *task, T_TTOS_TaskState taskState)
{
    T_UWORD state = (T_UWORD)TTOS_TASK_READY;
    /* @KEEP_COMMENT:  如果任务不是就绪任务，则直接设置状态 */
    /* @REPLACE_BRACKET: TTOS_TASK_READY != (task->state & TTOS_TASK_READY) */
    if (state != (task->state & state))
    {
        task->state |= (T_UWORD)taskState;
        return;
    }

    task->state = (T_UWORD)taskState;
    (void)ttosClearTaskReady(task);
}

/*
 * @brief:
 *    获取当前正在运行任务的名字。
 * @return:
 *    正在运行任务名字的指针。
 * @implements: RTE_DTASK.40.19
 */
T_UBYTE *ttosGetRunningTaskName(void)
{
    /* @REPLACE_BRACKET: ttosGetRunningTask()->objCore.objName */
    return (ttosGetRunningTask()->objCore.objName);
}

/*
 * @brief:
 *    获取当前CPU上正在运行的任务。
 * @return:
 *    当前CPU上正在运行任务的控制块。
 * @implements: RTE_DTASK.40.20
 */
T_TTOS_TaskControlBlock *ttosGetCurrentCpuRunningTask(void)
{
    /* @REPLACE_BRACKET: ttosGetRunningTask() */
    return (ttosGetRunningTask());
}

/*
 * @brief:
 *    获取当前CPU上的IDLE任务。
 * @return:
 *    当前CPU上的IDLE任务的控制块。
 * @implements: RTE_DTASK.40.21
 */
T_TTOS_TaskControlBlock *ttosGetIdleTask(void)
{
#if defined(CONFIG_SMP)
#if CONFIG_SMP == 1
    /* @REPLACE_BRACKET: ttosManager.idleTask[cpuid_get()] */
    return (ttosManager.idleTask[cpuid_get()]);
#else
    /* @REPLACE_BRACKET: ttosManager.idleTask */
    return (ttosManager.idleTask);
#endif
#else  /*defined(CONFIG_SMP)*/
    /* @REPLACE_BRACKET: ttosManager.idleTask */
    return (ttosManager.idleTask);
#endif /*defined(CONFIG_SMP)*/
}

/*
 * @brief:
 *    获取指定CPU上的IDLE任务。
 * @param[in]: cpuID: CPU ID，仅仅多核才使用此参数
 * @return:
 *    指定CPU上的IDLE任务。
 * @implements: RTE_DTASK.40.22
 */
T_TTOS_TaskControlBlock *ttosGetIdleTaskWithCpuID(T_UWORD cpuID)
{
#if defined(CONFIG_SMP)
#if CONFIG_SMP == 1
    /* @REPLACE_BRACKET: ttosManager.idleTask[cpuID] */
    return (ttosManager.idleTask[cpuID]);
#else
    /* @REPLACE_BRACKET: ttosManager.idleTask */
    return (ttosManager.idleTask);
#endif
#else  /*defined(CONFIG_SMP)*/
    /* @REPLACE_BRACKET: ttosManager.idleTask */
    return (ttosManager.idleTask);
#endif /*defined(CONFIG_SMP)*/
}

#if defined(CONFIG_SMP)
#if CONFIG_SMP == 1
/*
 * @brief:
 *    获取非预留CPU上可抢占的最低优先级的运行任务。
 * @return:
 *    非预留CPU上可抢占的最低优先级的运行任务。
 * @implements: RTE_DTASK.40.23
 */
T_TTOS_TaskControlBlock *ttosGetUnReserveCpuLowestRTask(T_VOID)
{
    T_TTOS_TaskControlBlock *lowestTask = NULL;
    T_UWORD i;
    T_TTOS_TaskControlBlock *runningTask;

    /* @REPLACE_BRACKET: i =U(0); i < TTOS_CONFIG_CPUS(); i++ */
    for (i = U(0); i < TTOS_CONFIG_CPUS(); i++)
    {
        runningTask = ttosGetRunningTaskWithCpuID(i);

        /* @REPLACE_BRACKET: 只需要检查非预留CPU上的运行任务*/
        if ((U(0) == (CPU_ISSET(i, TTOS_CPUSET_RESERVED()))) &&
            (U(0) != (CPU_ISSET(i, TTOS_CPUSET_ENABLED()))) && (U(0) == runningTask->preemptCount))
        {
            /* @REPLACE_BRACKET: NULL == lowestTask*/
            if (NULL == lowestTask)
            {
                lowestTask = runningTask;
            }

            /* @REPLACE_BRACKET:  lowestTask->taskCurPriority <
             * runningTask->taskCurPriority */
            if (lowestTask->taskCurPriority < runningTask->taskCurPriority)
            {
                lowestTask = runningTask;
            }
        }
    }

    /* @REPLACE_BRACKET: lowestTask */
    return (lowestTask);
}

/*
 * @brief:
 *   判断就绪任务的优先级是否比指定CPU上的运行任务优先级更高。
 * @param[in]: task: 任务控制块
 * @param[in]: cpuID: CPU ID
 * @return:
 *    FALSE: 就绪任务的优先级小于等于指定CPU上的运行任务优先级。
 *    TRUE: 就绪任务的优先级大于指定CPU上的运行任务优先级。
 * @implements: RTE_DTASK.40.24
 */
T_BOOL isRTaskNeedReSchedule(T_TTOS_TaskControlBlock *task, T_UWORD cpuID)
{
    T_TTOS_TaskControlBlock *runningTask = ttosGetRunningTaskWithCpuID(cpuID);

    /* @REPLACE_BRACKET: 比较任务优先级 */
    if (U(0) != (CPU_ISSET(cpuID, TTOS_CPUSET_ENABLED())) &&
        (task->taskCurPriority < runningTask->taskCurPriority) &&
        ((U(0) == runningTask->preemptCount)))
    {
        /* @REPLACE_BRACKET: TRUE */
        return (TRUE);
    }

    else
    {
        /* @REPLACE_BRACKET: FALSE */
        return (FALSE);
    }
}

/*
 * @brief:
 *    任务从从非就绪变为就绪时，或者任务从就绪变为非就绪时，或者运行任务的优先级
 *发生改变时，检查是否需要对非当前CPU发送重调度IPI。
 * @param[in]: task: 任务控制块
 * @param[in]: isChangedToReady:
 *TRUE表示任务从从非就绪变为就绪或者运行任务的优先级发送改变;
 *                                                 FALSE表示任务从就绪变为非就绪。
 * @return:
 *    无
 * @implements: RTE_DTASK.40.25
 */
T_VOID ttosTaskStateChanged(T_TTOS_TaskControlBlock *task, BOOL isChangedToReady)
{
    T_UWORD currentCpuID = cpuid_get();
    T_UWORD rescheduleCpuID;
    rescheduleCpuID = currentCpuID;

    /* @REPLACE_BRACKET: TRUE == isChangedToReady */
    if (TRUE == (unsigned int)isChangedToReady)
    {
        /* @REPLACE_BRACKET:
         * 任务变为就绪时或者运行任务优先级改变时的需要考虑对非当前CPU重调度的影响。*/
        if (CPU_NONE != task->smpInfo.affinityCpuIndex)
        {
            if (TRUE == isRTaskNeedReSchedule(task, task->smpInfo.affinityCpuIndex))
            {
                /* @KEEP_COMMENT:  <task>为绑定任务*/
                rescheduleCpuID = task->smpInfo.affinityCpuIndex;
            }
        }

        else
        {
            /* @KEEP_COMMENT:  <task>为非绑定任务*/
            /* @REPLACE_BRACKET: CPU_NONE != task->smpInfo.cpuIndex */
            if (CPU_NONE != task->smpInfo.cpuIndex)
            {
                /* @KEEP_COMMENT:  非绑定的<task>为运行任务*/
                rescheduleCpuID = task->smpInfo.cpuIndex;
            }

            else
            {
                /* @KEEP_COMMENT:  非绑定的<task>为就绪任务*/
                /* @REPLACE_BRACKET: (U(0) !=
                 * (!CPUSET_ISSET(TTOS_CPUSET_RESERVED(), currentCpuID))) &&
                 * (TRUE == isRTaskNeedReSchedule(task, currentCpuID)) */
                if ((U(0) == (CPU_ISSET(currentCpuID, TTOS_CPUSET_RESERVED()))) &&
                    (TRUE == isRTaskNeedReSchedule(task, currentCpuID)))
                {
                    /* @KEEP_COMMENT:
                     * 非绑定的就绪任务在就绪队列上有改变时，优先考虑非预留的当前CPU。*/
                    rescheduleCpuID = currentCpuID;
                }

                else
                {
                    /* @KEEP_COMMENT:
                     * 非绑定的就绪任务在就绪队列上有改变时，检查是否需要重调度。*/
                    T_TTOS_TaskControlBlock *lowestTask;
                    lowestTask = ttosGetUnReserveCpuLowestRTask();

                    /* @REPLACE_BRACKET: (NULL != lowestTask) &&
                     * (task->taskCurPriority < lowestTask->taskCurPriority)*/
                    if ((NULL != lowestTask) &&
                        (task->taskCurPriority < lowestTask->taskCurPriority))
                    {
                        /*
                         *由于前面已经考虑当前CPU的重调度条件，所以运行到此处时，lowestTask->smpInfo.cpuIndex一定
                         *不为当前CPU。
                         *rescheduleCpuID为非预留CPU上最低优先级可抢占的运行任务所在的CPU。
                         */
                        rescheduleCpuID = lowestTask->smpInfo.cpuIndex;
                    }
                }
            }
        }
    }

    else
    {
        /* @KEEP_COMMENT:  从就绪队列删除时的需要考虑对非当前CPU重调度的影响。*/
        /* @REPLACE_BRACKET: CPU_NONE != task->smpInfo.cpuIndex */
        if (CPU_NONE != task->smpInfo.cpuIndex)
        {
            /* @KEEP_COMMENT:  <task>为运行任务*/
            rescheduleCpuID = task->smpInfo.cpuIndex;
        }

        else
        {
            /* @KEEP_COMMENT:
             * <task>为非运行任务，非运行任务从就绪变为非就绪时，不会影响CPU的重调度。*/
            rescheduleCpuID = currentCpuID;
        }
    }

    /* @KEEP_COMMENT:
     * 当前CPU在接口退出时会进行任务的重调度，不需要调用重调度接口。*/
    /* @REPLACE_BRACKET: currentCpuID != rescheduleCpuID */
    if (currentCpuID != rescheduleCpuID)
    {
        cpu_set_t rescheduleCpuSet;
        memset(&rescheduleCpuSet, 0, sizeof(cpu_set_t));
        CPU_SET(rescheduleCpuID, &rescheduleCpuSet);
        (T_VOID) ipiRescheduleEmit(&rescheduleCpuSet, TRUE);
    }
}

/*
 * @brief:
 *    将全局任务放入就绪队列中。
 * @param[out]||[in]: task: 任务控制块
 * @return:
 *    无
 * @notes:
 *    本接口在全局运行任务被切走时调用。
 * @implements: RTE_DTASK.40.26
 */
T_VOID ttosGTaskRQueuePut(T_TTOS_TaskControlBlock *task)
{
    T_TTOS_TaskControlBlock *lowestTask;
    T_UWORD state = (T_UWORD)TTOS_TASK_READY;

    /* @REPLACE_BRACKET: CPU_NONE != task->smpInfo.affinityCpuIndex*/
    if (CPU_NONE != task->smpInfo.affinityCpuIndex)
    {
        /* <task>是绑定任务*/
        return;
    }

    /* @KEEP_COMMENT:  全局任务切走时，就认为任务已经轮转过了。*/
    task->smpInfo.isRotated = FALSE;

    /* @REPLACE_BRACKET: TRUE == task->smpInfo.isChangedToPrivate*/
    if (TRUE == task->smpInfo.isChangedToPrivate)
    {
        task->smpInfo.affinity = task->smpInfo.gRunTaskAffinity;
        task->smpInfo.affinityCpuIndex = task->smpInfo.gRunTaskAffinityCpuIndex;
        task->smpInfo.isChangedToPrivate = FALSE;
        task->smpInfo.gRunTaskAffinityCpuIndex = CPU_NONE;
    }

    /* @REPLACE_BRACKET: U(0) == (task->state & TTOS_TASK_READY)*/
    if (U(0) == (task->state & state))
    {
        /* 非绑定任务<task>是非就绪态*/
        return;
    }

    /*
     * @KEEP_COMMENT:
     * 设置<task>任务的优先级在优先级位图ttosPriorityBitMap中的相应位
     * 设置优先级位图，并将Task加入到其优先级所在的就绪队列尾部
     */
    TBSP_SET_PRIORITYBITMAP(task->smpInfo.affinityCpuIndex, task->taskCurPriority);
    /* @KEEP_COMMENT:
     * 将<task>任务插入对应优先级就绪队列ttosManager.priorityQueue尾部 */
    ttosReadyQueuePut(task, FALSE);
    /* @KEEP_COMMENT:
     * 重新获取最高优先级任务，保存于ttosManager.priorityQueueTask */
    ttosSetHighestPriorityTask(task->smpInfo.affinityCpuIndex);

    /*
     *非绑定的运行任务由全局任务改为绑定时，在任务切换时通过执行ttosGTaskRQueuePut()
     *重新放入绑定就绪队列，在运行到切回任务时，如果全局任务绑定的CPU为当前CPU，
     *会通过执行ttosGetCandidateTask()再次参与调度。
     */
    /* @REPLACE_BRACKET: CPU_NONE == task->smpInfo.affinityCpuIndex */
    if (CPU_NONE == task->smpInfo.affinityCpuIndex)
    {
        lowestTask = ttosGetUnReserveCpuLowestRTask();

        /* @KEEP_COMMENT:
         * 由于task是当前CPU上被切走的全局任务，所以lowestTask一定不为task。*/
        /* @REPLACE_BRACKET: (NULL != lowestTask) && (task->taskCurPriority <
         * lowestTask->taskCurPriority)*/
        if ((NULL != lowestTask) && (task->taskCurPriority < lowestTask->taskCurPriority))
        {
            /* @KEEP_COMMENT: 向需要改变调度的cpu发送重调度 IPI中断*/
            cpu_set_t set;
            CPU_ZERO(&set);
            CPU_SET(lowestTask->smpInfo.cpuIndex, &set);
            (T_VOID) ipiRescheduleEmit(&set, TRUE);
        }
    }

    else
    {
        ttosTaskStateChanged(task, TRUE);
    }
}

/*
 * @brief:
 *    将全局任务从就绪队列中摘除。
 * @param[out]||[in]: task: 任务控制块
 * @return:
 *    无
 * @notes:
 *    本接口在全局任务选为运行任务时调度。
 * @implements: RTE_DTASK.40.27
 */
T_VOID ttosGTaskRQueueRemove(T_TTOS_TaskControlBlock *task)
{
    /* @REPLACE_BRACKET: CPU_NONE != task->smpInfo.affinityCpuIndex*/
    if (CPU_NONE != task->smpInfo.affinityCpuIndex)
    {
        return;
    }

    /* @KEEP_COMMENT:
     * 调用list_delete(DT.8.8)将<task>任务从任务优先级就绪队列中移除 */
    list_delete(&(task->objCore.objectNode));
    /* 优化会导致指令重新排序，添加编译屏障保证顺序排列指令 */
    TTOS_COMPILER_MEMORY_BARRIER();

    /* @REPLACE_BRACKET: <task>任务所在的优先级就绪队列为空 */
    if (TRUE == ttosReadyQueueIsEmpty(task->smpInfo.affinityCpuIndex, task->taskCurPriority))
    {
        /* @KEEP_COMMENT:
         * 清除<task>任务的优先级在优先级位图中ttosPriorityBitMap对应的位 */
        TBSP_CLEAR_PRIORITYBITMAP(task->smpInfo.affinityCpuIndex, task->taskCurPriority);
    }

    /* @KEEP_COMMENT:
     * 重新获取最高优先级任务，保存于ttosManager.priorityQueueTask */
    ttosSetHighestPriorityTask(task->smpInfo.affinityCpuIndex);
}

/**
 * @brief:
 *    设置指定任务的亲和力。
 * @param[in]: taskID:
 * 任务的ID，当为TTOS_SELF_OBJECT_ID时表示设置当前任务的亲和力
 * @param[in]: affinity: 指定任务需要设置的亲和力，为0时，表示指定任务是全局
 *                               的，任务可以在任意一个CPU上运行
 * @return:
 *    TTOS_CALLED_FROM_ISR: 在虚拟中断处理程序中执行此接口。
 *    TTOS_INVALID_NUMBER: <affinity>表示亲和的CPU不存在或者需要亲和多个CPU；
 *                                      需要亲和的CPU与指定任务当前亲和的CPU是相同的。
 *    TTOS_INVALID_ID: <taskID>所表示的对象类型不是任务；
 *                     不存在<taskID>指定的任务。
 *    TTOS_INVALID_STATE: <taskID>是运行任务，但是此任务是不可抢占的。
 *    TTOS_OK: 设置指定任务的亲和力成功。
 * @implements: RTE_DTASK.40.28
 */
T_TTOS_ReturnCode TTOS_SetTaskAffinity(TASK_ID taskID, cpu_set_t *affinity)
{
    TBSP_MSR_TYPE msr = 0U;
    T_TTOS_TaskControlBlock *task;
    T_TTOS_TaskControlBlock *executing = ttosGetRunningTask();
    T_UWORD affinityCpuIndex = 0;
    T_UWORD oldState;
    T_UWORD oldTickSlice;
    T_UWORD state = (T_UWORD)TTOS_TASK_READY;

    /* @REPLACE_BRACKET: TRUE == ttosIsISR()*/
    if (TRUE == ttosIsISR())
    {
        /* 在虚拟中断处理程序中执行此接口*/
        /* @REPLACE_BRACKET:  TTOS_CALLED_FROM_ISR*/
        return (TTOS_CALLED_FROM_ISR);
    }

    /* @REPLACE_BRACKET: U(0) == affinity*/
    if (CPU_COUNT(affinity) == 0)
    {
        /* @KEEP_COMMENT: affinity为0时，表示指定任务是非绑定的。*/
        affinityCpuIndex = CPU_NONE;
    }

    else
    {
        /* @KEEP_COMMENT: affinity不为0时，表示指定任务是绑定的。*/
        for (int i = 0; i < CPU_SETSIZE; i++)
        {
            if (CPU_ISSET(i, affinity))
            {
                affinityCpuIndex = i;
                break;
            }
        }

        /* @KEEP_COMMENT: 任务只允许最多亲和在一个CPU上。*/
        /* @REPLACE_BRACKET: (affinityCpuIndex >= TTOS_CONFIG_CPUS ()) ||
         * (CPU_COUNT(affinity) > 1) */
        if ((affinityCpuIndex >= TTOS_CONFIG_CPUS()) || (CPU_COUNT(affinity) > 1))
        {
            /* 需要亲和的CPU不存在或者需要亲和多个CPU。*/
            /* @REPLACE_BRACKET: TTOS_INVALID_NUMBER */
            return (TTOS_INVALID_NUMBER);
        }

        /* @REPLACE_BRACKET: (U(0) != (!CPUSET_ISSET(TTOS_CPUSET_ENABLED(),
         * affinityCpuIndex))) || (affinityCpuIndex >= CONFIG_MAX_CPUS)*/
        if ((U(0) == (CPU_ISSET(affinityCpuIndex, TTOS_CPUSET_ENABLED()))) ||
            (affinityCpuIndex >= CONFIG_MAX_CPUS))
        {
            /* 指定的CPU不存在。*/
            /* @REPLACE_BRACKET: TTOS_INVALID_NUMBER */
            return (TTOS_INVALID_NUMBER);
        }
    }

    /* @REPLACE_BRACKET: (TASK_ID)TTOS_SELF_OBJECT_ID == taskID */
    if ((TASK_ID)TTOS_SELF_OBJECT_ID == taskID)
    {
        /* @REPLACE_BRACKET: TRUE == ttosIsIdleTask(executing) */
        if (TRUE == ttosIsIdleTask(executing))
        {
            /* 当前任务是IDLE任务 */
            /* @REPLACE_BRACKET: TTOS_INVALID_USER */
            return (TTOS_INVALID_USER);
        }

        /* @KEEP_COMMENT: 获取当前运行任务的ID */
        taskID = executing->taskControlId;
    }

    /* @KEEP_COMMENT: 获取<taskID>指定的任务存*/
    task = (T_TTOS_TaskControlBlock *)ttosGetObjectById(taskID, TTOS_OBJECT_TASK);

    /* @REPLACE_BRACKET: NULL == task */
    if ((0ULL) == task)
    {
        /* task任务不存在 或者是其它核上的IDLE任务*/
        /* @REPLACE_BRACKET: TTOS_INVALID_ID */
        return (TTOS_INVALID_ID);
    }

    /* @REPLACE_BRACKET: affinity == task->smpInfo.affinity */
    if (CPU_EQUAL(affinity, &task->smpInfo.affinity))
    {
        /* @KEEP_COMMENT: 需要亲和的CPU与指定任务当前亲和的CPU是相同的。*/
        ttosEnableTaskDispatchWithLock();
        /* @REPLACE_BRACKET: TTOS_OK */
        return (TTOS_OK);
    }

    /* @REPLACE_BRACKET: (CPU_NONE != task->smpInfo.cpuIndex) &&
     * ((task->preemptCount > U(0)) */
    if ((CPU_NONE != task->smpInfo.cpuIndex) && ((task->preemptCount > U(0))))
    {
        /* @KEEP_COMMENT: 设置指定运行任务的亲和力时，任务是不可抢占的。*/
        ttosEnableTaskDispatchWithLock();
        /* @REPLACE_BRACKET: TTOS_INVALID_STATE */
        return (TTOS_INVALID_STATE);
    }

    /* @KEEP_COMMENT: 禁止虚拟中断 */
    TBSP_GLOBALINT_DISABLE(msr);
    /* @REPLACE_BRACKET: (CPU_NONE == task->smpInfo.affinityCpuIndex) &&
     * (CPU_NONE != task->smpInfo.cpuIndex) */
    if ((CPU_NONE == task->smpInfo.affinityCpuIndex) && (CPU_NONE != task->smpInfo.cpuIndex))
    {
        /*
         *全局运行任务在发生切换时，才能放入就绪队列，避免多个核同时运行一个任务。
         *非绑定的运行任务由全局任务改为绑定时，在任务切换时通过执行ttosGTaskRQueuePut()
         *重新放入绑定就绪队列，在运行到切回任务时，如果全局任务绑定的CPU为当前CPU，
         *会通过执行ttosGetCandidateTask()再次参与调度。
         */
        task->smpInfo.isChangedToPrivate = TRUE;
        CPU_ZERO(&task->smpInfo.gRunTaskAffinity);
        CPU_OR(&task->smpInfo.gRunTaskAffinity, &task->smpInfo.gRunTaskAffinity, affinity);
        task->smpInfo.gRunTaskAffinityCpuIndex = affinityCpuIndex;
    }

    else
    {
        oldState = task->state;
        oldTickSlice = task->tickSlice;

        /* @REPLACE_BRACKET: TTOS_TASK_READY == ( oldState & TTOS_TASK_READY) */
        if (state == (oldState & state))
        {
            /* @KEEP_COMMENT: 将指定任务从原有就绪队列中摘除*/
            ttosClearTaskReady(task);
        }

        CPU_ZERO(&task->smpInfo.affinity);
        CPU_OR(&task->smpInfo.affinity, &task->smpInfo.affinity, affinity);
        task->smpInfo.affinityCpuIndex = affinityCpuIndex;

        /* @REPLACE_BRACKET: TTOS_TASK_READY == ( oldState & TTOS_TASK_READY) */
        if (state == (oldState & state))
        {
            /* @KEEP_COMMENT: 将指定任务插入亲和属性对应的就绪队列。*/
            ttosSetTaskReady(task);
            /* @KEEP_COMMENT: 恢复指定任务原有的时间片。*/
            task->tickSlice = oldTickSlice;
        }
    }

    trace_task_bindcpu(taskID);

    /* @KEEP_COMMENT: 使能虚拟中断*/
    TBSP_GLOBALINT_ENABLE(msr);
    ttosEnableTaskDispatchWithLock();
    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}

/**
 * @brief:
 *    获取指定任务的亲和力。
 * @param[in]: taskID:
 * 任务的ID，当为TTOS_SELF_OBJECT_ID时表示设置当前任务的亲和力
 * @param[out]: affinity: 指定任务的亲和力
 * @return:
 *    TTOS_INVALID_ADDRESS: <affinity>为空。
 *    TTOS_INVALID_ID: <taskID>所表示的对象类型不是任务；
 *                     不存在<taskID>指定的任务。
 *    TTOS_OK: 获取指定任务的亲和力成功。
 * @implements: RTE_DTASK.40.29
 */
T_TTOS_ReturnCode TTOS_GetTaskAffinity(TASK_ID taskID, cpu_set_t *affinity)
{
    T_TTOS_TaskControlBlock *task;
    T_TTOS_TaskControlBlock *executing = ttosGetRunningTask();

    /* @REPLACE_BRACKET: NULL == affinity */
    if (NULL == affinity)
    {
        /* @REPLACE_BRACKET: TTOS_INVALID_ADDRESS */
        return (TTOS_INVALID_ADDRESS);
    }

    /* @REPLACE_BRACKET: (TASK_ID)TTOS_SELF_OBJECT_ID == taskID */
    if ((TASK_ID)TTOS_SELF_OBJECT_ID == taskID)
    {
        /* @KEEP_COMMENT: 获取当前运行任务的ID */
        taskID = executing->taskControlId;
    }

    /* @KEEP_COMMENT: 获取<taskID>指定的任务存*/
    task = (T_TTOS_TaskControlBlock *)ttosGetObjectById(taskID, TTOS_OBJECT_TASK);

    /* @REPLACE_BRACKET: NULL == task */
    if ((0ULL) == task)
    {
        /* task任务不存在 或者是其它核上的IDLE任务*/
        /* @REPLACE_BRACKET: TTOS_INVALID_ID */
        return (TTOS_INVALID_ID);
    }

    *affinity = task->smpInfo.affinity;
    ttosEnableTaskDispatchWithLock();
    /* @REPLACE_BRACKET: TTOS_OK */
    return (TTOS_OK);
}

/*
 * @brief:
 *    判断所有启动CPU上的运行和候选任务是否都是idle任务。
 * @return:
 *    TRUE: 所有启动CPU上的运行和候选任务是idle任务。
 *    FALSE: 所有启动CPU上的运行和候选任务不全是idle任务。
 * @implements: RTE_DTASK.40.30
 */
T_BOOL TTOS_IsAllRunningAndCandidateTaskIdleTask(T_VOID)
{
    T_UWORD i;
    T_TTOS_TaskControlBlock *runningTask;

    /* @REPLACE_BRACKET: i =U(0); i < TTOS_CONFIG_CPUS(); i++ */
    for (i = U(0); i < TTOS_CONFIG_CPUS(); i++)
    {
        runningTask = ttosGetRunningTaskWithCpuID(i);

        /* @REPLACE_BRACKET: (U(0) != (CPUSET_ISSET(TTOS_CPUSET_ENABLED(),i)))
           &&
                ((ttosManager.idleTask[i] != runningTask)
           ||(ttosManager.idleTask[i] != ttosGetCandidateTask(i))) */
        if ((U(0) != (CPU_ISSET(i, TTOS_CPUSET_ENABLED()))) &&
            ((ttosManager.idleTask[i] != runningTask) ||
             (ttosManager.idleTask[i] != ttosGetCandidateTask(i))))
        {
            /* @REPLACE_BRACKET: FALSE */
            return (FALSE);
        }
    }

    /* @REPLACE_BRACKET: TRUE */
    return (TRUE);
}

#else  /*CONFIG_SMP*/
/*单核模式下，不存在发送重调度IPI，实现为空。*/
void ttosTaskStateChanged(T_TTOS_TaskControlBlock *task, BOOL isChangedToReady) {}
#endif /*CONFIG_SMP*/
#else  /*#if defined(CONFIG_SMP)*/
/*单核模式下，不存在发送重调度IPI，实现为空。*/
void ttosTaskStateChanged(T_TTOS_TaskControlBlock *task, BOOL isChangedToReady) {}
#endif /*#if defined(CONFIG_SMP)*/

int *ttos_task_errno(void)
{
    static int __errno__;

    T_TTOS_TaskControlBlock *task = ttosGetRunningTask();

    return task ? &task->taskErrno : &__errno__;
}
