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
 * @file     ttosBase.h
 * @brief
 *    <li>提供TTOS相关的宏定义、类型定义和接口声明</li>
 * @implements：DT
 */

#ifndef _TTOSBASE_H
#define _TTOSBASE_H

/************************头文件********************************/
#include <ttos.h>
#include <ttosInterHal.h>
#include <ttosTypes.h>
#include <ttosUtils.h>

#ifdef __cplusplus
extern "C"
{
#endif

/************************宏定义********************************/
/*-----------------TTOS的一些基本参数定义---------------------*/
/* 定义栈的安全保留空间 */
#define TTOS_SAFE_STACK_RESERVE_SIZE U (16)

/*------------------优先级位图相关操作定义----------------*/
#ifdef TTOS_32_PRIORTIY
#define TBSP_SET_PRIORITYBITMAP(pri)                                           \
    ({ ttosPriorityBitMap |= ((U (0x80000000)) >> (pri)); })

#define TBSP_CLEAR_PRIORITYBITMAP(pri)                                         \
    ({ ttosPriorityBitMap &= (~((U (0x80000000)) >> (pri))); })
#else /*TTOS_32_PRIORTIY*/
#if CONFIG_SMP == 1
/* 优先级位图置位 */
#define TBSP_SET_PRIORITYBITMAP(cpuID, pri)                                    \
    {                                                                          \
        T_UWORD setPri = (T_UWORD)pri >> U (5);                                \
        if (CPU_NONE == cpuID)                                                 \
        {                                                                      \
            ttosGlobalPriorityBitMap.bitMap[setPri]                            \
                |= ((U (0x80000000)) >> ((T_UWORD)pri & U (0x1F)));            \
            ttosGlobalPriorityBitMap.major |= ((U (0x80000000)) >> (setPri));  \
        }                                                                      \
        else                                                                   \
        {                                                                      \
            ttosPriorityBitMap[cpuID].bitMap[setPri]                           \
                |= ((U (0x80000000)) >> ((T_UWORD)pri & U (0x1F)));            \
            ttosPriorityBitMap[cpuID].major |= ((U (0x80000000)) >> (setPri)); \
        }                                                                      \
    }

/* 优先级位图复位 */
#define TBSP_CLEAR_PRIORITYBITMAP(cpuID, pri)                                  \
    {                                                                          \
        T_UWORD clePri = (T_UWORD)pri >> U (5);                                \
        if (CPU_NONE == cpuID)                                                 \
        {                                                                      \
            ttosGlobalPriorityBitMap.bitMap[clePri]                            \
                &= (~((U (0x80000000)) >> ((T_UWORD)pri & U (0x1F))));         \
            if (ttosGlobalPriorityBitMap.bitMap[clePri] == 0U)                 \
            {                                                                  \
                ttosGlobalPriorityBitMap.major                                 \
                    &= (~((U (0x80000000)) >> (clePri)));                      \
            }                                                                  \
        }                                                                      \
        else                                                                   \
        {                                                                      \
            ttosPriorityBitMap[cpuID].bitMap[clePri]                           \
                &= (~((U (0x80000000)) >> ((T_UWORD)pri & U (0x1F))));         \
            if (ttosPriorityBitMap[cpuID].bitMap[clePri] == 0U)                \
            {                                                                  \
                ttosPriorityBitMap[cpuID].major                                \
                    &= (~((U (0x80000000)) >> (clePri)));                      \
            }                                                                  \
        }                                                                      \
    }
#else /*CONFIG_SMP*/
/* 优先级位图置位 */
#define TBSP_SET_PRIORITYBITMAP(cpuID, pri)                                    \
    {                                                                          \
        T_UWORD setPri = (T_UWORD)pri >> U (5);                                \
        ttosPriorityBitMap.bitMap[setPri]                                      \
            |= ((U (0x80000000)) >> ((T_UWORD)pri & U (0x1F)));                \
        ttosPriorityBitMap.major |= ((U (0x80000000)) >> (setPri));            \
    }

/* 优先级位图复位 */
#define TBSP_CLEAR_PRIORITYBITMAP(cpuID, pri)                                  \
    {                                                                          \
        T_UWORD clePri = (T_UWORD)pri >> U (5);                                \
        ttosPriorityBitMap.bitMap[clePri]                                      \
            &= (~((U (0x80000000)) >> ((T_UWORD)pri & U (0x1F))));             \
        if (ttosPriorityBitMap.bitMap[clePri] == 0U)                           \
        {                                                                      \
            ttosPriorityBitMap.major &= (~((U (0x80000000)) >> (clePri)));     \
        }                                                                      \
    }
#endif /*CONFIG_SMP*/
#endif /*TTOS_32_PRIORTIY*/

/* IDLE 任务的名称 */
#define TTOS_IDLETASK_NAME "Idle"

/* 处理任务删除自身的任务名称 */
#define TTOS_DELETE_HANDLER_TASK_NAME "Releaser"

/*定义系统默认创建的任务个数，当前只有处理任务删除自身的任务。*/
#define TTOS_DEFAULT_CREATE_TASK_NUM U (1)

#define TTOS_IDLETASK_SLICESIZE 0xFFFFFFFF /* IDLE任务的缺省时间片 */

/* 任务未处于运行状态标志 */
#define TTOS_TASK_NONRUNNING_STATE                                             \
    (TTOS_TASK_DORMANT | TTOS_TASK_WAITING | TTOS_TASK_SUSPEND                 \
     | TTOS_TASK_PWAITING | TTOS_TASK_CWAITING)

/* 对象类型和序号 */
#define TTOS_OBJECT_TYPE_MASK  ((T_UWORD)0xFF000000)
#define TTOS_OBJECT_INDEX_MASK ((T_UWORD)0x00FFFFFF)

#define TTOS_OBJECT_MALLOC_DEFAULT_SIZE U (0x10)

#define TTOS_TASK_DELETE_SELF_EVENT TTOS_EVENT_31

/************************类型定义******************************/
/* 对象类型及其相关屏蔽码定义 */
typedef enum
{
    /* 任务对象类型 */
    TTOS_OBJECT_TASK = 0,

    /* 信号量对象类型 */
    TTOS_OBJECT_SEMA = 1,

    /* 定时器对象类型 */
    TTOS_OBJECT_TIMER = 2,

    /* 消息队列对象类型 */
    TTOS_OBJECT_MSGQ = 3,
} T_TTOS_ObjectType;

/* 系统全局对象链表首队列 */
#define TTOS_OBJECT_CHAIN_FIRST TTOS_OBJECT_TASK

/* 系统全局对象链表尾队列 */
#define TTOS_OBJECT_CHAIN_LAST TTOS_OBJECT_MSGQ

/* 定义周期任务周期等待队列或者周期任务截止时间等待队列的通知类型*/
typedef enum
{
    /* 不通知周期任务周期和截止时间等待队列*/
    TTOS_PERIOD_NONE_QUEUE_NOTIFY = 0x0,

    /* 通知周期任务周期等待队列*/
    TTOS_PERIOD_WAIT_QUEUE_NOTIFY = 0x1,

    /* 通知周期任务周期截止时间等待队列*/
    TTOS_PERIOD_EXPIRE_QUEUE_NOTIFY = 0x2,

    /* 通知周期任务周期和截止时间等待队列*/
    TTOS_PERIOD_ALL_QUEUE_NOTIFY = 0x3,
} T_TTOS_PeriodQueueNotifyType;

/*TTOS管理结构定义*/
typedef struct {
#if CONFIG_SMP == 1
    /*指向DeltaTT的当前运行任务*/
    T_TTOS_TaskControlBlock *runningTask[CONFIG_MAX_CPUS];

    /*最高优先级的绑定任务，在每次调整优先级调度的绑定运行任务时修改该指针变量*/
    T_TTOS_TaskControlBlock *priorityQueueTask[CONFIG_MAX_CPUS];

    /*最高优先级的非绑定任务，在每次调整优先级调度的非绑定运行任务时修改该指针变量*/
    T_TTOS_TaskControlBlock *globalTask;

    /*指向DeltaTT中的IDLE任务*/
    T_TTOS_TaskControlBlock *idleTask[CONFIG_MAX_CPUS];
#else
    /*指向DeltaTT的当前运行任务*/
    T_TTOS_TaskControlBlock *runningTask;

    /*优先级调度的当前运行任务，在每次调整优先级调度的当前运行任务时修改该指针变量*/
    T_TTOS_TaskControlBlock *priorityQueueTask;

    /*指向DeltaTT中的IDLE任务*/
    T_TTOS_TaskControlBlock *idleTask;
#endif /*CONFIG_SMP*/

    /*任务对象的控制结构表首地址，在DeltaTT初始化时赋值，运行过程中不会修改*/
    T_TTOS_TaskControlBlock *taskCB;

    /*信号量对象的控制结构表首地址，在DeltaTT初始化时赋值，运行过程中不会修改*/
    T_TTOS_SemaControlBlock *semaCB;

#ifdef TTOS_MSGQ
    /*消息队列对象的控制结构表首地址，在DeltaTT初始化时赋值，运行过程中不会修改*/
    T_TTOS_MsgqControlBlock *msgqCB;
#endif

    /*定时器对象的控制结构表首地址，在DeltaTT初始化时赋值，运行过程中不会修改*/
    T_TTOS_TimerControlBlock *timerCB;

    /* 任务自己删除自身的队列 */
    struct list_head deleteSelfTaskIdQueue;

    /*任务tick等待队列链表头*/
    struct list_head taskWaitedQueue;

    /*定时器等待队列链表头*/
    struct list_head timerWaitedQueue;

    /*周期等待队列链表头*/
    struct list_head periodWaitedQueue;

    /*截止期等待队列链表头*/
    struct list_head expireWaitedQueue;

#if CONFIG_SMP == 1
    /*绑定任务优先级就绪队列链表头，即私有就绪队列链表头，每个优先级一个链表头*/
    struct list_head *priorityQueue[CONFIG_MAX_CPUS];

    /*非绑定任务优先级就绪队列链表头，即全局就绪队列链表头，每个优先级一个链表头*/
    struct list_head globalPriorityQueue[TTOS_PRIORITY_NUMBER];
#else
    /*任务优先级就绪队列链表头，每个优先级一个链表头*/
    struct list_head priorityQueue[TTOS_PRIORITY_NUMBER];
#endif /*CONFIG_SMP*/

    /* 系统允许创建的最大对象个数: 任务、定时器、信号量、消息队列 */
    T_UWORD objMaxNumber[TTOS_OBJECT_CHAIN_LAST + 1];

    /* 系统中已启用的对象个数: 任务、定时器、信号量、消息队列 */
    T_UWORD objExistNumber[TTOS_OBJECT_CHAIN_LAST + 1];

    /* 全局空闲的对象资源: 任务、定时器、信号量、消息队列 */
    struct list_head inactiveResource[TTOS_OBJECT_CHAIN_LAST + 1];

    /* 全局正在使用的对象资源: 任务、定时器、信号量、消息队列 */
    struct list_head inUsedResource[TTOS_OBJECT_CHAIN_LAST + 1];

    /* 全局正在使用的对象资源个数: 任务、定时器、信号量、消息队列 */
    T_UWORD inUsedResourceNum[TTOS_OBJECT_CHAIN_LAST + 1];

} T_TTOS_TTOSManager;

#ifndef TTOS_32_PRIORTIY
/*优先级位图类型定义*/
typedef struct {
    T_UWORD major;     /* 优先级主索引号，只用低8位 */
    T_UWORD bitMap[8]; /* 位图表，0级最高，255最低 */

} T_TTOS_PriorityMap;
#endif

/************************外部声明******************************/
/* ttos控制结构 */
T_EXTERN T_TTOS_TTOSManager ttosManager;

/*记录是否已经开始初始化*/
T_EXTERN T_BOOL ttosInitialized;

T_VOID ttosInsertWaitedTimer (T_TTOS_TimerControlBlock *timer);
T_VOID ttosExactWaitedTimer (T_TTOS_TimerControlBlock *timer);
T_VOID ttosClearTaskWaiting (T_TTOS_TaskControlBlock *task);
T_VOID ttosSetTaskReady (T_TTOS_TaskControlBlock *task);
T_VOID ttosClearTaskReady (T_TTOS_TaskControlBlock *task);
T_VOID ttosTaskStateChanged (T_TTOS_TaskControlBlock *task,
                             BOOL                     isChangedToReady);

#if CONFIG_SMP == 1
T_VOID ttosGTaskRQueuePut (T_TTOS_TaskControlBlock *task);
T_VOID ttosGTaskRQueueRemove (T_TTOS_TaskControlBlock *task);
#endif

T_VOID  ttosRotateRunningTask (T_TTOS_TaskControlBlock *task);
T_VOID  ttosInsertWaitedTask (T_TTOS_TaskControlBlock *task);
T_VOID  ttosExactWaitedTask (T_TTOS_TaskControlBlock *task);
T_UWORD ttosInsertWaitedPeriodTask (T_TTOS_PeriodTaskNode *task,
                                    struct list_head      *chain,
                                    T_UWORD                waited_time);
T_UWORD ttosExactWaitedPeriodTask (T_TTOS_PeriodTaskNode *task,
                                   struct list_head      *chain,
                                   T_UWORD                waited_time);
T_VOID  ttosSchedule (T_VOID);
T_VOID  ttosScheduleInKernelLock (T_VOID);
T_VOID  ttosScheduleInIntDisAndKernelLock (TBSP_MSR_TYPE msr);
T_TTOS_TaskControlBlock *ttosGetCandidateTask (T_UWORD cpuID);
T_BOOL                   ttosIsNeedRescheduleWithCpuId (T_UWORD cpuID);
T_VOID                   ttosTickNotify (T_UWORD ticks);
T_VOID                   ttosTimerNotify (T_UWORD ticks);
T_VOID                   ttosPeriodWaitNotify (T_VOID);
T_VOID                   ttosPeriodExpireNotify (T_VOID);
T_VOID                   ttosPeriodWaitQueueModify (T_UWORD pwaiteLeftTicks);
T_VOID                   ttosPeriodExpireQueueModify (T_UWORD pexpireLeftTicks);
T_TTOS_PeriodQueueNotifyType ttosPeriodQueueMerge (T_VOID);

T_VOID ttosDisableTaskDispatch (T_VOID);
T_VOID ttosEnableTaskDispatch (T_VOID);
T_VOID ttosDisableTaskDispatchWithLock (T_VOID);
T_VOID ttosEnableTaskDispatchWithLock (T_VOID);

T_BOOL ttosVerifyName (T_UBYTE *name, T_UWORD length);

T_VOID ttosChangeName (T_UBYTE *name1, T_UBYTE *Name2);

T_VOID ttosCopyString (T_UWORD *dest, T_UWORD *src, T_UWORD size);
T_VOID ttosInitTask (T_TTOS_TaskControlBlock *taskCB,
                     T_TTOS_ConfigTask *taskConfig, TASK_ID *taskID);
T_VOID ttosInitTaskParam (T_TTOS_TaskControlBlock *taskCB);
T_VOID ttosStopTask (T_TTOS_TaskControlBlock *taskCB);
T_VOID ttosTaskExit (T_TTOS_TaskControlBlock *taskCB, T_VOID *exitValue);
T_VOID ttosSuspendTask (T_TTOS_TaskControlBlock *taskCB);
T_VOID ttosSetTaskState (T_TTOS_TaskControlBlock *task,
                         T_TTOS_TaskState         taskState);
T_VOID ttosSetPWaitAndExpireTick ();
T_VOID ttosInitSema (T_TTOS_SemaControlBlock *semaCB,
                     T_TTOS_ConfigSema *semaConfig, T_BOOL isPriorityCeiling);
T_VOID ttosInitTimer (T_TTOS_TimerControlBlock *timerCB,
                      T_TTOS_ConfigTimer       *timerConfig);
T_TTOS_ReturnCode ttosInitMsgq (T_TTOS_MsgqControlBlock *msgqCB,
                                T_TTOS_ConfigMsgq *msgqConfig, MSGQ_ID *msgqID);
T_VOID            ttosDeleteMessage (T_TTOS_MsgqControlBlock *theMessageQueue);
T_VOID            ttosCommitMessage (T_TTOS_MsgqControlBlock *theMessageQueue,
                                     T_TTOS_MsgqBufControl *theMessage, T_VOID *buffer,
                                     T_UWORD size, T_UWORD isUrgent, TBSP_MSR_TYPE msr);
T_VOID            ttosInsertMsg (T_TTOS_MsgqControlBlock *theMessageQueue,
                                 T_TTOS_MsgqBufControl *theMessage, T_UWORD isUrgent);
T_VOID            taskChangePriority (T_TTOS_TaskControlBlock *theTask,
                                      T_UBYTE newPriority, T_BOOL prependIt);
T_VOID            ttosEnqueueTaskq (T_TTOS_Task_Queue_Control *theTaskQueue,
                                    T_UWORD                    ticks, T_BOOL is_interruptible);
T_VOID            ttosEnqueueTaskqEx (T_TTOS_TaskControlBlock *task, T_TTOS_Task_Queue_Control *theTaskQueue, T_UWORD ticks, T_BOOL is_interruptible);

T_VOID ttosEnqueueFifo (struct list_head *chain, struct list_node *node);
T_VOID ttosEnqueuePriority (T_TTOS_Task_Queue_Control *theTaskQueue,
                            T_TTOS_TaskControlBlock   *theTask);

T_TTOS_TaskControlBlock *
ttosDequeueTaskq (T_TTOS_Task_Queue_Control *theTaskQueue);
T_TTOS_TaskControlBlock *
ttosDequeueTaskFromTaskq (T_TTOS_Task_Queue_Control *theTaskQueue);
T_TTOS_TaskControlBlock *
ttosDequeueFifo (T_TTOS_Task_Queue_Control *theTaskQueue);
T_TTOS_TaskControlBlock *
ttosDequeuePriority (T_TTOS_Task_Queue_Control *theTaskQueue);

T_VOID ttosInitializeTaskq (T_TTOS_Task_Queue_Control    *theTaskQueue,
                            T_TTOS_Task_Queue_Disciplines theDiscipline,
                            T_TTOS_TaskState              state);
T_BOOL ttosTaskqIsEmpty (T_TTOS_Task_Queue_Control *theTaskQueue);
T_VOID ttosFlushTaskq (T_TTOS_Task_Queue_Control *theTaskQueue, UINT32 status);
T_VOID ttosExtractTaskq (T_TTOS_Task_Queue_Control *theTaskQueue,
                         T_TTOS_TaskControlBlock   *theTask);
T_VOID ttosInitObjectCore (T_VOID *objectID, T_TTOS_ObjectType objectType);
T_TTOS_ObjectCoreID ttosGetObjectById (T_VOID           *objectID,
                                       T_TTOS_ObjectType objectType);
T_BOOL ttosObjectTypeIsOk (T_VOID *objectID, T_TTOS_ObjectType objectType);
T_TTOS_ObjectCoreID ttosGetIdByName (T_UBYTE          *objectCoreName,
                                     T_TTOS_ObjectType objectType);
T_UWORD ttosGetIdList (T_TTOS_ObjectCoreID *ttosArea, UINT32 areaSize,
                       T_TTOS_ObjectType objectType);
T_TTOS_ObjectCoreID
       ttosGetObjectFromInactiveResource (T_TTOS_ObjectType objectType);
T_VOID ttosReturnObjectToInactiveResource (T_VOID           *objectID,
                                           T_TTOS_ObjectType objectType,
                                           T_BOOL            isExtracted);
T_VOID ttosInvalidObjectCore (T_TTOS_ObjectCore *objectCore);
T_TTOS_ReturnCode ttosAllocateObject (T_TTOS_ObjectType objectType,
                                      T_VOID          **objectID);

#if CONFIG_SMP == 1
T_VOID ttosKernelLockInit (T_VOID);
T_BOOL ttosIsKernelLockOwner (T_UWORD cpuID);
T_BOOL isRTaskNeedReSchedule (T_TTOS_TaskControlBlock *task, T_UWORD cpuID);
#endif

T_TTOS_ReturnCode TTOS_GetTaskState (TASK_ID taskID, T_TTOS_TaskState *state);

#ifdef CONFIG_PROTECT_STACK
T_TTOS_ReturnCode ttosStackUnprotect (TASK_ID taskID);
T_TTOS_ReturnCode ttosStackProtect (TASK_ID taskID);
#endif /* CONFIG_PROTECT_STACK */
/*
 *几乎每个文件都会引用ttosBase.h，此处显示的引用ttosInterTask.inl是为了避免
 *其他文件再显示引用ttosInterTask.inl。
 */
#include <ttosInterTask.inl>

#ifdef __cplusplus
}
#endif

#endif
