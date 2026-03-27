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

/*
 * @file:   ttosTask.inl
 * @brief:
 *    <li>提供任务优先级相关的内联函数实现。</li>
 * @implements: DK.2
 */

/************************头 文 件******************************/
#ifndef _TTOS_INTER_TASK_INL
#define _TTOS_INTER_TASK_INL

/* @<MOD_HEAD */
#include <ttosInterHal.h>
#include <ttosTypes.h>
#include <ttosUtils.inl>
/* @MOD_HEAD> */

#ifdef __cplusplus
extern "C" {
#endif

/************************宏 定 义******************************/
/************************外部声明******************************/

/* @<MOD_EXTERN */

#ifdef TTOS_32_PRIORTIY
/* 优先级位图 */
T_EXTERN T_UWORD ttosPriorityBitMap;
#else /*TTOS_32_PRIORTIY*/
#if CONFIG_SMP == 1
/*绑定任务优先级就绪队列对应的位图*/
T_EXTERN T_TTOS_PriorityMap ttosPriorityBitMap[CONFIG_MAX_CPUS];

/*非绑定任务优先级就绪队列对应的位图*/
T_EXTERN T_TTOS_PriorityMap ttosGlobalPriorityBitMap;
#else  /*CONFIG_SMP*/
T_EXTERN T_TTOS_PriorityMap ttosPriorityBitMap;
#endif /*CONFIG_SMP*/
#endif /*TTOS_32_PRIORTIY*/

/* ttos控制结构 */
T_EXTERN T_TTOS_TTOSManager ttosManager;

/* 调度开关 */
#if CONFIG_SMP == 1
T_EXTERN T_ULONG ttosDisableScheduleLevel[CONFIG_MAX_CPUS];
#else
T_EXTERN T_ULONG ttosDisableScheduleLevel;
#endif

T_EXTERN T_VOID *running_task[CONFIG_MAX_CPUS];
/* @MOD_EXTERN> */

/************************实   现*******************************/

/* @MODULE> */

/*
 * @brief:
 *    获取优先级位图中最高优先级。
 * @param[in]: cpuID: CPU ID，仅仅多核才使用此参数
 * @return:
 *    优先级位图中最高优先级
 * @tracedREQ: RT.2
 * @implements: DT.2.34
 */
T_INLINE T_UWORD ttosGetHighestPriority(T_UWORD cpuID)
{
#ifdef TTOS_32_PRIORTIY
    T_UWORD __pri = 0;

    /* @KEEP_COMMENT: 获取优先级位图ttosPriorityBitMap中最高优先级存放至变量__pri */
    __pri = zero_num_of_word_get(ttosPriorityBitMap);

    /* @REPLACE_BRACKET: __pri */
    return (__pri);
#else /*TTOS_32_PRIORTIY*/
    T_UWORD pri = 0;
    T_UWORD getPri = 0;

#if CONFIG_SMP == 1
    if (CPU_NONE == cpuID)
    {
        /* 获取优先级主索引号 */
        getPri = zero_num_of_word_get(ttosGlobalPriorityBitMap.major);

        /* 获取优先级位图主索引号指定的项值 */
        pri = zero_num_of_word_get(ttosGlobalPriorityBitMap.bitMap[getPri]);
    }
    else
    {
        /* 获取优先级主索引号 */
        getPri = zero_num_of_word_get(ttosPriorityBitMap[cpuID].major);

        /* 获取优先级位图主索引号指定的项值 */
        pri = zero_num_of_word_get(ttosPriorityBitMap[cpuID].bitMap[getPri]);
    }
#else  /*CONFIG_SMP*/
    /* 获取优先级主索引号 */
    getPri = zero_num_of_word_get(ttosPriorityBitMap.major);

    /* 获取优先级位图主索引号指定的项值 */
    pri = zero_num_of_word_get(ttosPriorityBitMap.bitMap[getPri]);
#endif /*CONFIG_SMP*/

    /* 最高优先级 */
    return (pri | (getPri << 5));
#endif /*TTOS_32_PRIORTIY*/
}
/* @IGNORE_BEGIN: */
/*
 * @brief:
 *    获取优先级就绪队列的头结点。
 * @param[in]: cpuID: CPU ID，仅仅多核才使用此参数
 * @param[in]: pri: 任务优先级
 * @return:
 *    FALSE: 链表非空。
 *    TRUE: 链表为空。
 */
T_INLINE struct list_node *ttosReadyQueueHeadGet(T_UWORD cpuID, T_UBYTE pri)
{
    struct list_node *node;

#if CONFIG_SMP == 1
    if (CPU_NONE == cpuID)
    {
        node = list_first(&(ttosManager.globalPriorityQueue[pri]));
    }
    else
    {
        node = list_first(&(ttosManager.priorityQueue[cpuID][pri]));
    }
#else
    node = list_first(&(ttosManager.priorityQueue[pri]));
#endif

    return (node);
}
/* @IGNORE_END: */
/*
 * @brief:
 *    获取任务优先级就绪队列中最高优先级任务。
 * @param[in]: cpuID: CPU ID，仅仅多核才使用此参数
 * @return:
 *    任务优先级就绪队列中最高优先级任务控制块
 * @tracedREQ: RT.2
 * @implements: DT.2.36
 */
T_INLINE T_TTOS_TaskControlBlock *ttosGetHighestPriorityTask(T_UWORD cpuID)
{
    T_UWORD pri;

#if CONFIG_SMP == 1
    if (CPU_NONE == cpuID)
    {
        if (0 == ttosGlobalPriorityBitMap.major)
        {
            /*
             *非绑定任务的优先级主索引号为0时，表示无就绪态的全局任务。
             *
             *每个cpu上都有绑定的IDLE任务，所以每个CPU的绑定任务的优先级主索引号为0
             *一定不为0。
             */
            return (NULL);
        }
    }
#endif

    /* @KEEP_COMMENT: 调用ttosGetHighestPriority(DT.2.34)获取优先级位图中最高优先级存放至变量pri */
    pri = ttosGetHighestPriority(cpuID);

    /* @REPLACE_BRACKET: 任务优先级就绪队列ttosManager.priorityQueue中优先级为pri的首个任务 */
    return ((T_TTOS_TaskControlBlock *)(ttosReadyQueueHeadGet(cpuID, pri)));
}
/* @IGNORE_BEGIN: */
/*
 * @brief:
 *    设置指定CPU的任务优先级就绪队列中最高优先级任务。
 * @param[in]: cpuID: 需要最高优先级任务的CPU，仅仅多核才使用此参数
 * @return:
 *    无。
 */
T_INLINE T_VOID ttosSetHighestPriorityTask(T_UWORD cpuID)
{
#if CONFIG_SMP == 1
    if (CPU_NONE == cpuID)
    {
        ttosManager.globalTask = ttosGetHighestPriorityTask(cpuID);
    }
    else
    {
        ttosManager.priorityQueueTask[cpuID] = ttosGetHighestPriorityTask(cpuID);
    }
#else
    ttosManager.priorityQueueTask = ttosGetHighestPriorityTask(cpuID);
#endif
}

/*
 * @brief:
 *    将就绪任务加入就绪队列。
 * @param[in]: task: 任务控制块
 * @param[in]: prependIt: 任务是否需要需要优先考虑
 * @return:
 *    无。
 */
T_INLINE T_VOID ttosReadyQueuePut(T_TTOS_TaskControlBlock *task, T_BOOL prependIt)
{
    if (TRUE == prependIt)
    {
#if CONFIG_SMP == 1
        if (CPU_NONE == task->smpInfo.affinityCpuIndex)
        {
            /* 非绑定任务就绪时，加入全局优先级就绪队列。*/
            list_add_head(&(task->objCore.objectNode),
                          &(ttosManager.globalPriorityQueue[task->taskCurPriority]));
        }
        else
        {
            /* 绑定任务就绪时，加入私有优先级就绪队列。*/
            list_add_head(&(task->objCore.objectNode),
                          &(ttosManager.priorityQueue[task->smpInfo.affinityCpuIndex]
                                                     [task->taskCurPriority]));
        }
#else
        list_add_head(&(task->objCore.objectNode),
                      &(ttosManager.priorityQueue[task->taskCurPriority]));
#endif
    }
    else
    {
#if CONFIG_SMP == 1
        if (CPU_NONE == task->smpInfo.affinityCpuIndex)
        {
            /* 非绑定任务就绪时，加入全局优先级就绪队列。*/
            list_add_tail(&(task->objCore.objectNode),
                          &(ttosManager.globalPriorityQueue[task->taskCurPriority]));
        }
        else
        {
            /* 绑定任务就绪时，加入私有优先级就绪队列。*/
            list_add_tail(&(task->objCore.objectNode),
                          &(ttosManager.priorityQueue[task->smpInfo.affinityCpuIndex]
                                                     [task->taskCurPriority]));
        }
#else
        list_add_tail(&(task->objCore.objectNode),
                      &(ttosManager.priorityQueue[task->taskCurPriority]));
#endif
    }
}

/*
 * @brief:
 *    判断就绪队列是否为空。
 * @param[in]: cpuID: CPU ID，仅仅多核才使用此参数
 * @param[in]: pri: 任务优先级
 * @return:
 *    FALSE: 链表非空。
 *    TRUE: 链表为空。
 */
T_INLINE T_BOOL ttosReadyQueueIsEmpty(T_UWORD cpuID, T_UBYTE pri)
{
    BOOL ret = FALSE;

#if CONFIG_SMP == 1
    if (CPU_NONE == cpuID)
    {
        if (TRUE == list_is_empty(&(ttosManager.globalPriorityQueue[pri])))
        {
            ret = TRUE;
        }
    }
    else
    {
        if (TRUE == list_is_empty(&(ttosManager.priorityQueue[cpuID][pri])))
        {
            ret = TRUE;
        }
    }
#else
    if (TRUE == list_is_empty(&(ttosManager.priorityQueue[pri])))
    {
        ret = TRUE;
    }
#endif

    return (ret);
}

/*
 * @brief:
 *    获取优先级就绪队列。
 * @param[in]: cpuID: CPU ID，仅仅多核才使用此参数
 * @param[in]: pri: 任务优先级
 * @return:
 *    FALSE: 链表非空。
 *    TRUE: 链表为空。
 */
T_INLINE struct list_head *ttosReadyQueueGet(T_UWORD cpuID, T_UBYTE pri)
{
    struct list_head *chain;

#if CONFIG_SMP == 1
    if (CPU_NONE == cpuID)
    {
        chain = &(ttosManager.globalPriorityQueue[pri]);
    }
    else
    {
        chain = &(ttosManager.priorityQueue[cpuID][pri]);
    }
#else
    chain = &(ttosManager.priorityQueue[pri]);
#endif

    return (chain);
}
/* @IGNORE_END: */
/*
 * @brief:
 *    获取当前CPU上正在运行的任务。
 * @return:
 *    当前CPU上正在运行任务的控制块。
 * @tracedREQ: RT.2
 * @implements: DT.2.35
 */
T_INLINE T_TTOS_TaskControlBlock *ttosGetRunningTask(T_VOID)
{
#if CONFIG_SMP == 1

    /*
     *由于非绑定任务在获取CPUID后获取运行任务前可能切换，那么获取的
     *运行任务就不是当前CPU的，所以在获取运行任务过程中需要保证任务
     *是不能被切走的，既保证获取运行任务过程是原子性的。
     */
    T_TTOS_TaskControlBlock *runningTask;
    TBSP_MSR_TYPE msr = 0;
    TBSP_GLOBALINT_DISABLE(msr);
    runningTask = ttosManager.runningTask[cpuid_get()];
    TBSP_GLOBALINT_ENABLE(msr);
    return (runningTask);
#else
    /* @REPLACE_BRACKET: ttosManager.runningTask */
    return (ttosManager.runningTask);
#endif
}

/*
 * @brief:
 *    获取指定CPU上正在运行的任务。
 * @param[in]: cpuID: CPU ID，仅仅多核才使用此参数
 * @return:
 *    当前CPU上正在运行任务的控制块。
 * @tracedREQ: RT.2
 * @implements: DT.2.37
 */
T_INLINE T_TTOS_TaskControlBlock *ttosGetRunningTaskWithCpuID(T_UWORD cpuID)
{
#if CONFIG_SMP == 1
    return (ttosManager.runningTask[cpuID]);
#else
    /* @REPLACE_BRACKET: ttosManager.runningTask */
    return (ttosManager.runningTask);
#endif
}
/* @IGNORE_BEGIN: */
/*
 * @brief:
 *    设置当前CPU上正在运行的任务。
 * @param[in]: cpuID: CPU ID，仅仅多核才使用此参数
 * @param[in]: task: 任务控制块
 * @return:
 *    无。
 */
T_INLINE T_VOID ttosSetRunningTaskWithCpuID(T_UWORD cpuID, T_TTOS_TaskControlBlock *task)
{
#if CONFIG_SMP == 1
    ttosManager.runningTask[cpuID] = task;
    running_task[cpuID] = task;
    ttosManager.runningTask[cpuID]->smpInfo.cpuIndex = cpuID;
#else
    ttosManager.runningTask = task;
    running_task[0] = task;
    ttosManager.runningTask->smpInfo.cpuIndex = cpuID;
#endif
}
/* @IGNORE_END: */
/*
 * @brief:
 *	  判断指定任务是否是idle任务。
 * @param[in]: taskID: 需要判断的任务ID
 * @return:
 *	  TRUE: 是idle任务。
 *	  FALSE: 不是idle任务。

 * @implements: DT.9.13
 */
T_INLINE T_BOOL ttosIsIdleTask(TASK_ID taskID)
{
    /*
     *1.IDLE任务本身是绑定的任务，在运行过程中不会从一个CPU迁移到
     *另外一个CPU。如果是IDLE任务调用此接口总是返回TRUE。
     *2.如果是绑定的任务调用此接口总是访问FALSE。
     *3.如果是非绑定的任务调用此接口，在获取ttosCpuIDGet()后是否发生
     *CPU迁移，总是返回FALSE。
     *4.综上，通过ttosCpuIDGet()访问IDLE任务时，不需要禁止中断来保证访
     *问IDLE任务的原子性。
     */
    if (taskID == ttosGetIdleTask())
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}
/* @END_HERE: */
/*
 * @brief:
 *    获取当前CPU上的禁止调度层次。
 * @return:
 *    禁止调度层次。
 */
T_INLINE T_UWORD ttosGetDisableScheduleLevel(T_VOID)
{
#if CONFIG_SMP == 1
    return (ttosDisableScheduleLevel[cpuid_get()]);
#else
    return (ttosDisableScheduleLevel);
#endif
}

/*
 * @brief:
 *    获取指定CPU上的禁止调度层次。
 * @param[in]: cpuID: CPU ID，仅仅多核才使用此参数
 * @return:
 *    禁止调度层次。
 */
T_INLINE T_UWORD ttosGetDisableScheduleLevelWithCpuID(T_UWORD cpuID)
{
#if CONFIG_SMP == 1
    return (ttosDisableScheduleLevel[cpuID]);
#else
    return (ttosDisableScheduleLevel);
#endif
}

/*
 * @brief:
 *   设置当前CPU上的禁止调度层次。
 * @param[in]: level: 需要设置禁止调度层次
 * @return:
 *    无。
 */
T_INLINE T_VOID ttosSetDisableScheduleLevel(T_UWORD cpuID, T_UWORD level)
{
#if CONFIG_SMP == 1
    ttosDisableScheduleLevel[cpuID] = level;
#else
    ttosDisableScheduleLevel = level;
#endif
}

/*
 * @brief:
 *   递增当前CPU上的禁止调度层次。
 * @return:
 *    无。
 */
T_INLINE T_VOID ttosIncDisableScheduleLevel(T_VOID)
{
#if CONFIG_SMP == 1
    TBSP_MSR_TYPE msr = 0;
    TBSP_GLOBALINT_DISABLE(msr);
    ttosDisableScheduleLevel[cpuid_get()]++;
    TBSP_GLOBALINT_ENABLE(msr);
#else
    ttosDisableScheduleLevel++;
#endif
}

/*
 * @brief:
 *   递减当前CPU上的禁止调度层次。
 * @param[in]: cpuID: CPU ID，仅仅多核才使用此参数
 * @return:
 *    无。
 */
T_INLINE T_VOID ttosDecDisableScheduleLevel(T_UWORD cpuID)
{
#if CONFIG_SMP == 1
    ttosDisableScheduleLevel[cpuID]--;
#else
    ttosDisableScheduleLevel--;
#endif
}

/*
 * @brief:
 *   递减并返回当前CPU上的禁止调度层次。
 * @param[in]: cpuID: CPU ID，仅仅多核才使用此参数
 * @return:
 *    禁止调度层次。
 */
T_INLINE T_UWORD ttosDecAndGetDisableScheduleLevel(T_UWORD cpuID)
{
#if CONFIG_SMP == 1
    /*
     *1.虚拟中断是针对子分区的。如果当前运行任务切换到其它核上运行时，只有可能是由于中断的产生
     *导致的任务切换。
     *2.当前 子分区核心态处理完中断返回到用户态时，会进行虚拟中断的投递，子分区处理完虚拟中断后，
     *会进行任务的重调度，不会导致任务重调度的延时处理，仅仅被切换到的CPU核会多执行一次ttosSchedule()，
     *多消耗几微秒。
     */
    ttosDisableScheduleLevel[cpuID]--;
    return (ttosDisableScheduleLevel[cpuID]);
#else
    ttosDisableScheduleLevel--;
    return (ttosDisableScheduleLevel);
#endif
}

/*
 * @brief:
 *	  判断taskID是否是运行任务。
 * @param[in]: taskID: 需要判断的任务ID
 * @return:
 *	  TRUE: taskID是运行任务。
 *	  FALSE: taskID不是运行任务。
 */
T_INLINE T_BOOL ttosIsRunningTask(TASK_ID taskID)
{
    /*此接口已经通过CPUID获取了运行任务，不需要再禁止中断来判断taskID是否是运行任务。*/
    if (taskID == ttosGetRunningTask()->taskControlId)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

#ifdef __cplusplus
}
#endif

#endif /*_TTOS_INTER_TASK_INL*/
