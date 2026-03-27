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
 * @file： ttos.h
 * @brief：
 *    <li>提供TTOS相关的宏定义、类型定义和接口声明。</li>
 * @implements： DT
 */

#ifndef _TTOS_H
#define _TTOS_H

/************************头文件********************************/
#include <arch/arch.h>
#include <list.h>
#include <spinlock.h>
#include <stdlib.h>
#include <string.h>
#include <ttosHal.h>

#include <process_signal.h>
#define _GNU_SOURCE 1
#include <sched.h>
#include <time.h>

#include <rt_mutex.h>

#include <tid.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/************************宏定义********************************/
#ifndef TTOS_COMPILER_MEMORY_BARRIER
#define TTOS_COMPILER_MEMORY_BARRIER() __asm__ volatile("" ::: "memory")
#endif

/*当前对象配置数据结构版本号 */
#define TTOS_OBJ_CONFIG_CURRENT_VERSION "1.0"

/* 匿名信号量默认名字 */
#define TTOS_DEFAULT_SEMA_NAME "defaultCreateSema6789"

/* 匿名定时器默认名字  */
#define TTOS_DEFAULT_TIMER_NAME "defaultCreateTimer6789"

/* 匿名定时器默认名字  */
#define TTOS_DEFAULT_MSGQ_NAME "defaultCreatMsgq6789"

/* 匿名任务名字 */
#define TTOS_DEFAULT_TASK_NAME "defaultCreateTask6789"

/*默认的任务时间片大小*/
#define TTOS_DEFAULT_TASK_SLICE_SIZE 100U

/* 当前版本长度 */
#define TTOS_OBJECT_VERSION_LENGTH U(32)

/* 虚拟中断栈大小 */
#define TBSP_STACK_SIZE (PAGE_SIZE * U(4))

#ifdef CONFIG_PROTECT_STACK
/*默认任务栈对齐大小 */
#define DEFAULT_TASK_STACK_ALIGN_SIZE (PAGE_SIZE)
#else
/*默认任务栈对齐大小 */
#define DEFAULT_TASK_STACK_ALIGN_SIZE U(128)
#endif

/*默认任务栈大小 */
#define DEFAULT_TASK_STACK_SIZE TTOS_MINIMUM_STACK_SIZE

/*无效的vector*/
#define TTOS_INVALID_VECTOR 0xFFFFFFFF

/* TTOS对象自身ID号 */
#define TTOS_SELF_OBJECT_ID ((T_VOID *)0xFFFFFFFFU)

/* IDLE 任务 ID号 ，便于shell通过此ID获取IDLE任务的信息。*/
#define TTOS_IDLE_TASK_ID ((T_VOID *)U(0xFFFFFFF0))

/* 定时器永久循环标志 */
#define TTOS_TIMER_LOOP_FOREVER U(0xFFFFFFFF)

/* 永久等待信号量的标志 */
#define TTOS_SEMA_WAIT_FOREVER U(0xFFFFFFFF)

/* 永久等待互斥锁的标志 */
#define TTOS_MUTEX_WAIT_FOREVER U(0xFFFFFFFF)

/* 永久等待标志 */
#define TTOS_WAIT_FOREVER U(0xFFFFFFFF)

/* 定时器永久触发标志 */
#define TTOS_TIMER_REPEAT_FOREVER U(0xFFFFFFFF)

/* 无效对象ID定义 */
#define TTOS_NULL_OBJECT_ID ((TASK_ID)U(0xFFFFFFFF))

/* 事件功能控制开关 */
#define TTOS_EVENT 1

/* 消息队列功能控制开关 */
#define TTOS_MSGQ 1

/* 信号量类型屏蔽码 */
#define TTOS_SEMAPHORE_MASK U(0x00000003)

/* 计数信号量 */
#define TTOS_COUNTING_SEMAPHORE U(0x00000001)

/* 互斥信号量，允许同一任务嵌套获取、只能拥有者释放、支持优先级天花板 */
#define TTOS_MUTEX_SEMAPHORE U(0x00000002)

/* 互斥量排队策略属性配置，值为attributeSet[6:5](第5到第6位组合值表示) */
#define TTOS_MUTEX_WAIT_PRIORITY_CEILING (0 << 5)
#define TTOS_MUTEX_WAIT_PRIORITY (1 << 5)
#define TTOS_MUTEX_WAIT_PRIORITY_INHERIT (2 << 5)
#define TTOS_MUTEX_WAIT_FIFO (3 << 5)
#define TTOS_MUTEX_WAIT_ATTR_MASK (3 << 5)

#define TTOS_INIT_COUNT U(0x00000000)  /* 初始化COUNT值 */
#define TTOS_INIT_ID U(0x00000000)     /* 初始化ID值 */
#define TTOS_INIT_OPTION U(0x00000000) /* 初始化OPTION值 */
#define TTOS_INIT_SIZE U(0x00000000)   /* 初始化SIZE值 */

#ifdef TTOS_EVENT
/* 永久等待事件的标志 */
#define TTOS_EVENT_WAIT_FOREVER U(0xFFFFFFFF)
#endif

#ifdef TTOS_MSGQ

/* 永久等待消息的标志 */
#define TTOS_MSGQ_WAIT_FOREVER U(0xFFFFFFFF)

/* 消息队列信息优先级屏蔽码 */
#define TTOS_MSGQ_PRIORITY_MASK U(0x000000F8)

/* 消息队列优消息先级 */
#define TTOS_MSGQ_PRIORITY_NUMBER U(32)
#define TTOS_MSGQ_GET_PRIORITY(options) ((((options)&TTOS_MSGQ_PRIORITY_MASK) >> U(3)))
#define TTOS_MSGQ_SET_PRIORITY(priority) ((((priority) << U(3)) & TTOS_MSGQ_PRIORITY_MASK))

#define TTOS_MSGQ_SET_PRIORITYBITMAP(priorityBitMap, pri)                                          \
    ({ (priorityBitMap) |= (((T_UWORD)U(0x80000000)) >> (pri)); })

#define TTOS_MSGQ_CLEAR_PRIORITYBITMAP(priorityBitMap, pri)                                        \
    ({ (priorityBitMap) &= (~(((T_UWORD)U(0x80000000)) >> (pri))); })

/* 消息队列的紧急发送属性 */
#define TTOS_URGENT U(0x00000004)
#define TTOS_NO_URGENT U(0x00000000)
#endif

typedef UINT32 T_TTOS_Attribute; /* 属性类型 */

/*------------------事件记录相关操作定义----------------*/
/*TTOS任务切换事件掩码 */
#define TTOS_RECORD_TASK_SWITCH_MASK 0x00000001U

/* TTOS任务切换事件 */
#define TTOS_EVENT_TASKSWITCH 8

/************************类型定义******************************/

/* 初始化错误类型 */
typedef enum
{
    /* 任务配置中配置的任务优先级超出支持范围 */
    TTOS_CONFIG_TASK_PRIORITY_ERROR = 0x1,

    /* 任务入口错误 */
    TTOS_CONFIG_ENTRY_ERROR = 0x2,

    /* 任务栈配置错误 */
    TTOS_CONFIG_STACK_SIZE_ERROR = 0x3,

    /* 任务周期配置错误 */
    TTOS_CONFIG_PERIOD_ERROR = 0x4,

    /* 定时器处理函数错误 */
    TTOS_CONFIG_TIMER_ERROR = 0x5,

    /*对象名字name符串中存在ASCII取值范围不在[32,126]之间的字符或name为空串或者空指针*/
    TTOS_CONFIG_NAME_ERROR = 0x5,

    /* TTOS初始化过程中其他错误类型 */
    TTOS_INIT_OTHER_ERROR = 0x6,

    /* TTOS初始化过程中分配内存空间失败 */
    TTOS_INIT_MALLOC_ERROR = 0x7,

    /* TTOS初始化过程中内核堆初始化失败 */
    TTOS_INIT_WORKSPACE_ERROR = 0x8,

    /* TTOS初始化配置消息队列中消息个数错误或互斥信号量初值不为0和1 */
    TTOS_CONFIG_COUNT_ERROR = 0x9,

    /* TTOS初始化配置消息队列中基本缓存单元大小或消息大小错误 */
    TTOS_CONFIG_SIZE_ERROR = 0xa,

    /* TTOS初始化配置消息队时分配消息队列资源失败 */
    TTOS_CONFIG_INIT_MSGQ_ERROR = 0xb,

    /* TTOS初始化配置信号量时属性不为 TTOS_COUNTING_SEMAPHORE 和
     * TTOS_MUTEX_SEMAPHORE*/
    TTOS_CONFIG_ATTRIBUTE_ERROR = 0xc,

    /* 使用GRUB启动TTOS映像时，但是TTOS映像又配置了需要使用TA调试TTOS映像*/
    TTOS_CONFIG_DEBUG_ERROR = 0xd,

    /* TTOS初始化过程中，创建系统任务失败*/
    TTOS_CONFIG_CREATE_TASK_ERROR = 0xd,

#if CONFIG_SMP == 1
    /* TTOS从CPU启动过程中，启动从CPU失败*/
    TTOS_START_CPU_ERROR = 0xe,
#endif
} T_TTOS_Init_Eerros_List;

/* 内部错误类型 */
typedef enum
{
    /* 工作空间分配失败 */
    TTOS_INTERNAL_WORKSPACE_ALLOCATION_ERROR = 0x1

} T_TTOS_Internal_Eerros_List;

typedef struct
{
    /*[out]: 系统允许创建的最大任务个数 */
    T_UWORD taskMaxNumber;

    /*[out]: 系统中正在使用的任务个数*/
    T_UWORD taskInusedNumber;

    /*[out]: 系统允许创建的最大定时器个数 */
    T_UWORD timerMaxNumber;

    /*[out]: 系统中正在使用的定时器个数 */
    T_UWORD timerInusedNumber;

    /*[out]: 系统允许创建的最大信号量个数 */
    T_UWORD semaMaxNumber;

    /*[out]: 系统中正在使用的信号量个数 */
    T_UWORD semaInusedNumber;

    /*[out]: 系统允许创建的最大消息队列个数 */
    T_UWORD msgqMaxNumber;

    /*[out]: 系统中正在使用的消息队列个数*/
    T_UWORD msgqInusedNumber;

} T_TTOS_KernelBasicInfo;

/* 虚拟中断类型 */
typedef enum
{
    /* tick中断 */
    VMK_VINT_TICK = 0x1,

    /* 外部中断 */
    VMK_VINT_EXINT = 0x2,

    /* 异常 */
    VMK_VINT_EXCEPTION = 0x4,

    /* 服务请求中断 */
    VMK_VINT_SERVICE = 0x8

} T_VMK_VintType;

/* 任务类型 */
typedef enum
{
    /* 非周期任务 */
    TTOS_SCHED_NONPERIOD = 0,

    /* 周期任务 */
    TTOS_SCHED_PERIOD = 1

} T_TTOS_TaskType;

/* 任务执行完成后返回操作类型 */
typedef enum
{
    /* 表示该任务执行完成后停止自己 */
    TTOS_TASK_STOP = 0,

    /* 表示该任务执行完成后重启自己 */
    TTOS_TASK_RESET = 1

} T_TTOS_TaskReturnAction;

/* 定时器状态 */
typedef enum
{
    /* 未安装态 */
    TTOS_TIMER_UNINSTALL = 1,

    /* 已安装态 */
    TTOS_TIMER_INSTALLED,

    /* 激活态 */
    TTOS_TIMER_ACTIVE,

    /* 停止态 */
    TTOS_TIMER_STOP

} T_TTOS_TimerState;

/* 健康监控对异常的处理动作 */
typedef enum
{
    /* 忽略当前故障 */
    TTOS_HM_ACTION_IGNORE = 0x001,

    /* 停止任务 */
    TTOS_HM_ACTION_HALTTASK = 0x002,

    /* 复位任务 */
    TTOS_HM_ACTION_RESETTASK = 0x004,

    /* 挂起任务 */
    TTOS_HM_ACTION_SUSPENDTASK = 0x008,

    /* 热启动 */
    TTOS_HM_ACTION_WARMSTART = 0x020,

    /* 冷启动 */
    TTOS_HM_ACTION_COLDSTART = 0x040,

    /* 停机 */
    TTOS_HM_ACTION_HALT = 0x080,

    /* 无限循环 */
    TTOS_HM_ACTION_ENDLESSLOOP = 0x100

} T_TTOS_HMAction;

/* 虚拟中断例程执行模式 */
typedef enum
{
    /* 用户附加模式 */
    TTOS_VINT_USERADDIN = 0,

    /* 用户自定义模式 */
    TTOS_VINT_USERDEFINE,

    TTOS_VINT_LASTMODE

} T_TTOS_VintMode;

/* 时间单位 */
typedef enum
{
    /* 单位ns */
    TTOS_NS_UNIT = 0,

    /* 单位us  */
    TTOS_US_UNIT,

    /* 单位ms */
    TTOS_MS_UNIT,

    /* 单位s  */
    TTOS_S_UNIT

} T_TTOS_TimeUnitType;

/* 设置定时器tick的方式 */
typedef enum
{
    /* 只设置定时器第一次触发的tick */
    TTOS_ONLY_FIRST = 0,

    /* 只设置定时器周期触发的tick  */
    TTOS_ONLY_PERIOD,

    /* 若定时器第一次未触发，则只设置第一次触发的tick，否者只设置定时器周期触发的tick
     */
    TTOS_FIRST_THEN_PERIOD,

    /* 设置定时器第一次触发的tick和周期触发的tick */
    TTOS_FIRST_AND_PERIOD,

    /* 结束判断值 */
    TTOS_TimerTickSetTypeEnd

} T_TTOS_TimerTickSetType;

/* 周期任务节点 */
typedef struct
{
    /* 周期任务节点 */
    struct list_node objectNode;

    /* 指向周期任务节点的指针 */
    struct list_node *task;

    /* 在一个周期的工作是否完成 */
    T_UWORD jobCompleted;

    /* 周期时间 */
    T_UWORD periodTime;

    /* 持续时间 */
    T_UWORD durationTime;

    /* 周期任务启动延迟时间 */
    T_UWORD delayTime;

    /* 补偿时间 */
    T_UWORD replenishTime;

    /*
     *对于周期等待时，是周期等待时间；对于截止期等待时，是持续时间。
     *便于采用统一的接口进行周期等待队列和截止期等待队列的队列操作
     */
    T_UWORD waitedTime;

    /* 周期完成次数(即调用TTOS_WaitPeriod的次数) */
    T_UDWORD jobCompletedCount;

    /* 周期任务的当前实际执行cnt */
    T_UDWORD periodexecutecnt;

    /* 周期任务完成的实际执行时间us */
    T_UDWORD periodexecutecompletetime;

    /* 周期任务的完成时间us(从被调度开始到调用TTOS_WaitPeriod之间的时间) */
    T_UDWORD periodcompletetime;

    T_UDWORD periodStarted;

    /* 周期任务每个周期开始的时间戳 */
    T_UDWORD periodStartedTimestamp;

    /* 周期任务进入的时间戳 */
    T_UDWORD exeStartedTimestamp;

    bool isTimeout;
} T_TTOS_PeriodTaskNode;

typedef T_VOID (*T_TTOS_TaskRoutine)(T_VOID *arg);

#ifdef TTOS_EVENT
typedef T_UWORD T_TTOS_Tid; /* task id */
typedef T_UWORD T_TTOS_Option;
typedef T_UWORD T_TTOS_Interval;
typedef T_UWORD T_TTOS_ObjectsId;

/*宏定义*/
#define TTOS_EVENT_CURRENT U(0)                             /* 获取当前事件的数据定义 */
#define TTOS_EVENT_SETS_NONE_CONDITION ((T_TTOS_EventSet)0) /* 定义没有事件等待条件的数据 */

/*类型定义*/
typedef T_UWORD T_TTOS_EventSet; /* 定义事件的数据类型*/

/* 定义事件管理数据类型 */
/* Identifier: COD-NuEgretOS-177 事件管理数据类型 (Trace to:
 * SSD-NuEgretOS-840) */
typedef struct
{
    struct list_head wait_list;
    T_TTOS_EventSet pendingEvents; /* 挂起的事件 */
} T_TTOS_EventControl;

#define TTOS_WAIT 0x00000000U    /* 等待资源 */
#define TTOS_NO_WAIT 0x00000001U /* 不等待资源 */

/* 队列排队属性 */
#define TTOS_FIFO (0x00000000U)
#define TTOS_PRIORITY (0x00000001U)

#define TTOS_EVENT_ALL 0x00000000U /* 表示等待所有的事件 */
#define TTOS_EVENT_ANY 0x00000002U /* 表示等待任一事件 */

/* 定义EVENT的值 */

#define TTOS_EVENT_0 0x00000001
#define TTOS_EVENT_1 0x00000002
#define TTOS_EVENT_2 0x00000004
#define TTOS_EVENT_3 0x00000008
#define TTOS_EVENT_4 0x00000010
#define TTOS_EVENT_5 0x00000020
#define TTOS_EVENT_6 0x00000040
#define TTOS_EVENT_7 0x00000080
#define TTOS_EVENT_8 0x00000100
#define TTOS_EVENT_9 0x00000200
#define TTOS_EVENT_10 0x00000400
#define TTOS_EVENT_11 0x00000800
#define TTOS_EVENT_12 0x00001000
#define TTOS_EVENT_13 0x00002000
#define TTOS_EVENT_14 0x00004000
#define TTOS_EVENT_15 0x00008000
#define TTOS_EVENT_16 0x00010000
#define TTOS_EVENT_17 0x00020000
#define TTOS_EVENT_18 0x00040000
#define TTOS_EVENT_19 0x00080000
#define TTOS_EVENT_20 0x00100000
#define TTOS_EVENT_21 0x00200000
#define TTOS_EVENT_22 0x00400000
#define TTOS_EVENT_23 0x00800000
#define TTOS_EVENT_24 0x01000000
#define TTOS_EVENT_25 0x02000000
#define TTOS_EVENT_26 0x04000000
#define TTOS_EVENT_27 0x08000000
#define TTOS_EVENT_28 U(0x10000000)
#define TTOS_EVENT_29 0x20000000
#define TTOS_EVENT_30 0x40000000
#define TTOS_EVENT_31 U(0x80000000)
#endif

/* 任务等待信息控制结构 */
typedef struct
{
    /* 等待对象ID号 */
    T_VOID *id;

    /* 通用数据项 */
    T_UWORD count;

    /* 等待任务的第一个返回参数 */
    T_VOID *returnArgument;

    /* 等待任务的第二个返回参数 */
    T_VOID *returnArgument1;

    /* 被信号唤醒的信息 */
    siginfo_t sig_info;

    /* 等待的条件信息 */
    T_UWORD option;

    /* 任务返回时的返回值 */
    T_TTOS_ReturnCode returnCode;

    /* 优先级等待方式时，相同优先级任务的等待队列 */
    struct list_head samePriQueue;

    T_TTOS_Task_Queue_Control *queue; /* 指向所在的任务等待队列  */
} T_TTOS_Task_Wait_Information;

/*任务控制块中多核信息的数据结构定义*/
typedef struct
{
    /*
     *任务绑定运行的CPU,
     *对应CPU的对应位置位，则表示可以绑定运行在对应*CPU上。
     *最多只有一位被置位。为0时，表示任务可以在任意一个非预留的CPU上运行。
     */
    cpu_set_t affinity;

    /*表示任务绑定运行在哪个CPU上 ，CPU_NONE表示此任务是非绑定运行任务*/
    T_UWORD affinityCpuIndex;

    /*此属性仅对运行任务有效，表示任务运行在哪个CPU上
     * ，CPU_NONE表示此任务是非运行任务*/
    T_UWORD cpuIndex;

    /* 运行的最后一个核 */
    T_UWORD last_running_cpu;

    /*表示全局运行任务是否设置为绑定任务。*/
    T_BOOL isChangedToPrivate;

    /*表示全局运行任务是否需要进行时间片轮转调度。*/
    T_BOOL isRotated;

    /*表示全局运行任务设置为绑定任务时，任务的绑定属性。*/
    cpu_set_t gRunTaskAffinity;

    /*表示全局运行任务设置为绑定任务时，任务绑定运行在哪个CPU上 。*/
    T_UWORD gRunTaskAffinityCpuIndex;
} T_TTOS_SmpInfo;

typedef struct T_TTOS_TaskControlBlock_Struct *TASK_ID;

/* 任务控制块 */
typedef struct T_TTOS_TaskControlBlock_Struct
{
    /* 管理对象 */
    T_TTOS_ObjectCore objCore;

    TASK_ID taskControlId;

    pid_t tid;

    /* 任务状态 */
    T_UWORD state;

    /* 任务的优先级，0－31级，0最高，31最低 */
    T_UBYTE taskPriority;

    /* 任务调度策略 */
    T_UBYTE taskSchedPolicy;

    /* 任务正在使用的优先级，0－31级，0最高，31最低 */
    T_UBYTE taskCurPriority;

    /* 0表示可抢占，非0表示不可抢占 */
    T_UWORD preemptCount;

    /* 任务初始可抢占属性，在配置时确定，运行时不会改变,0表示可抢占，非0表示不可抢占
     */
    T_UWORD preemptedConfig;

    /* 任务运行时的参数，传递给任务入口函数entry使用 */
    T_VOID *arg;

    /* 任务入口函数 */
    T_TTOS_TaskRoutine entry;

    /* 等待信息控制结构 */
    T_TTOS_Task_Wait_Information wait;

#ifdef TTOS_EVENT
    /* 任务事件句柄 */
    T_TTOS_EventControl *eventCB;

    /* 事件的等待节点 */
    struct list_node event_wait_node;
#endif

    /* 任务类型，分为周期任务和非周期任务 */
    T_TTOS_TaskType taskType;

    /* 任务休眠或者等待资源的时间*/
    T_UWORD waitedTicks;

    /* 任务时间片的初始大小值，单位为tick，在配置时确定，运行时不会改变 */
    T_UWORD tickSliceSize;

    /* 任务时间片的大小，单位为tick */
    T_UWORD tickSlice;

    /* 任务运行的时间 */
    struct timespec64 stime;

    /* 任务进入时的时刻点 */
    struct timespec64 stime_prev;

    /* 任务的启动时间 */
    struct timespec64 start_time;

    /* 周期任务节点 */
    T_TTOS_PeriodTaskNode periodNode;

    /* 任务的资源节点 */
    T_TTOS_ResourceTaskNode resourceNode;

    /* 任务上下文 */
    T_TBSP_TaskContext switchContext;

    /* 任务运行栈的起始地址 */
    T_UBYTE *stackStart;

    /* 任务最初始的栈大小，进行任务栈保护后，此大小大于任务实际可以使用的栈大小。
     */
    T_UWORD initialStackSize;

    /* 任务运行栈栈底 */
    T_UBYTE *stackBottom;

    /* 核心态栈 */
    T_UBYTE *kernelStackTop;

    /* 任务拥有的资源数 */
    T_UWORD resourceCount;

    /* 任务已经执行的tick数*/
    T_UDWORD executedTicks;

    /*是否在TTOS_ExitTaskHook()中释放任务栈 */
    T_BOOL isFreeStack;

    T_WORD taskErrno;

    /* 任务是否禁止删除的，任务拥有资源时，是不允许被删除的 */
    T_UWORD disableDeleteFlag;

    /* 任务是否已经被其他任务join */
    T_UWORD joinedFlag;

    /* 任务的多核信息 */
    T_TTOS_SmpInfo smpInfo;

    /* 进程句柄 */
    void *ppcb;

    /* try 的上下文 */
    struct list_head try_ctx_list;

    /* futex value3 */
    uint32_t futex_bitset;

    /* robust list (per-thread) */
    void __user *robust_list_head;
    unsigned long robust_list_len;

    /* 优先级继承锁 */

    /* rt_mutex 等待对象 */
    struct rt_mutex_waiter waiter;

    /* 持有的 rt_mutex 列表 */
    struct list_head held_rt_mutexs;

    /* 保护上述链表的自旋锁 */
    ttos_spinlock_t held_rt_mutexs_lock;

    /* 正在等待的rt_mutex */
    struct rt_mutex *blocked_on;

} T_TTOS_TaskControlBlock;

typedef struct T_TTOS_TaskInfo_struct
{
    /* 任务ID */
    T_ULONG id;

    /* 任务名字 */
    T_UBYTE name[TTOS_OBJECT_NAME_LENGTH + 1];

    /* 任务的优先级，0－31级，0最高，31最低 */
    T_UBYTE taskPriority;

    /* 任务状态 */
    T_UWORD state;

    /* 任务入口函数 */
    T_TTOS_TaskRoutine entry;

    /* 等待的条件信息 */
    T_UWORD option;

    /* 等待的对象名称 */
    T_UBYTE waitObjName[TTOS_OBJECT_NAME_LENGTH + 1];

    /* 等待对象ID号 */
    T_VOID *waitObjId;

    /* 任务已经执行的tick数*/
    T_UDWORD executedTicks;

    /*表示任务绑定运行在哪个CPU上 ，CPU_NONE表示此任务是非绑定运行任务*/
    T_UWORD affinityCpuIndex;

    /*此属性仅对运行任务有效，表示任务运行在哪个CPU上
     * ，CPU_NONE表示此任务是非运行任务*/
    T_UWORD cpuIndex;
} T_TTOS_TaskInfo;

typedef struct T_TTOS_SemaControlBlock_Struct *SEMA_ID;

/* 信号量控制块 */
typedef struct T_TTOS_SemaControlBlock_Struct
{
    /* 管理对象 */
    T_TTOS_ObjectCore objCore;

    /* 信号量属性，包括计数信号量和互斥信号量 */
    T_TTOS_Attribute attributeSet;

    /* 信号量ID */
    SEMA_ID semaControlId;

    /* 信号量的计数值，有符号，负值表示资源需求，正值表示可用资源数量 */
    T_WORD semaControlValue;

    /* 信号量是否支持优先级天花板，只有等待队列为优先级方式的互斥信号量才允许优先级天花板
     */
    T_UWORD isPriorityCeiling;

    /* 优先级天花板的优先级 */
    T_UBYTE priorityCeiling;

    /* 信号量的拥有者，对于计数信号量相当于上次获取成功的任务 */
    TASK_ID semHolder;

    /* 信号嵌套深度，用于记录互斥信号量被相同任务嵌套获取的深度 */
    T_UWORD nestCount;

    /* 等待信号量的任务队列 */
    T_TTOS_Task_Queue_Control waitQueue;

} T_TTOS_SemaControlBlock;

typedef struct T_TTOS_TimerControlBlock_Struct *TIMER_ID;

/* 定时器函数类型定义 */
typedef T_VOID (*T_TTOS_TimerRoutine)(TIMER_ID timerID, /* 执行此处理程序的定时器ID */
                                      T_VOID *arg /* 配置或者 安装的定时器处理程序参数 */
);

/* 定时器控制块 */
typedef struct T_TTOS_TimerControlBlock_Struct
{
    /* 管理对象 */
    T_TTOS_ObjectCore objCore;

    /* 定时器ID */
    TIMER_ID timerControlId;

    /* 定时器状态 */
    T_UWORD state;

    /* 定时器定时tick数 */
    T_UWORD ticks;

    /* 定时器等待时间 */
    T_UWORD waitedTicks;

    /* 定时器的触发次数 */
    T_UWORD repeatCount;

    /* 定时器当前还能触发的次数 */
    T_UWORD remainCount;

    /* 定时器处理函数的参数 */
    T_VOID *argument;

    /* 定时器被触发时的处理函数 */
    T_TTOS_TimerRoutine handler;

} T_TTOS_TimerControlBlock;

/* 定时器信息 */
typedef struct
{
    /*[out]: 定时器的触发间隔时间，单位为tick */
    T_UWORD ticks;

    /*[out]: 定时器被触发时的处理程序 */
    T_TTOS_TimerRoutine handler;

    /*[out]:定时器处理程序参数 */
    T_VOID *argument;

    /*[out]: 定时器的触发次数 */
    T_UWORD repeatCount;

} T_TTOS_TimerInfo;

/*----------------配置相关定义---------------------*/

/* 任务配置结构  */
typedef struct
{
    /*[in]: 任务的名字，最多为32个字符(不包含串尾结束符) */
    T_UBYTE cfgTaskName[TTOS_OBJECT_NAME_LENGTH + 1];

    /*[in]: 任务的优先级，0－31级，0最高，31最低 */
    T_UBYTE taskPriority;

    /*[in]: 任务初始化时是否自动启动，TRUE表示自动启动，FALSE表示不自动启动
     */
    T_UBYTE autoStarted;

    /*[in]: 任务是否可抢占，TRUE表示可抢占，FALSE表示不可抢占 */
    T_BOOL preempted;

    /*[in]: 任务入口函数，用户需保证该地址空间的有效性 */
    T_TTOS_TaskRoutine entry;

    /*[in]:
     * 任务运行时的参数，传递给任务入口函数entry使用，用户需保证该地址空间的有效性*/
    T_VOID *arg;

    /*[in]:任务可抢占情况下，任务可运行的时间片大小，单位为tick */
    T_UWORD tickSliceSize;

    /*[in]: 任务运行栈的大小 */
    T_UWORD stackSize;

    /*[in]: 任务类型，分为周期任务和非周期任务 */
    T_TTOS_TaskType taskType;

    /*[in]: 周期任务的周期时间，单位为tick */
    T_UWORD periodTime;

    /* [in]:周期任务在周期内的可持续时间，单位为tick */
    T_UWORD durationTime;

    /* [in]:周期任务启动时第一个周期延迟启动时间，单位为tick */
    T_UWORD delayTime;

    /*[in]:静态配置任务时，此变量没有任何意义，任务栈是来自于T_TTOS_ConfigTable传入的总的任务栈。用户需保证该地址空间的有效性*/
    T_UBYTE *taskStack;

    /*[in]:对象配置数据结构版本号 */
    T_UBYTE objVersion[TTOS_OBJECT_VERSION_LENGTH + 1];

    /*[in]:
     * 是否在TTOS_ExitTaskHook()中释放任务栈，对于静态配置任务需设置为FALSE
     * 。*/
    T_BOOL isFreeStack;

    T_BOOL isprocess;

    void *pcb;
} T_TTOS_ConfigTask;

#ifdef TTOS_MSGQ

/* 消息块结构定义 */
typedef struct
{
    /* 消息节点 */
    struct list_node Node;

    /* 消息缓存大小 */
    T_UWORD size;

    /* 消息优先级 */
    T_UBYTE priority;

    /* 消息缓存地址*/
    T_UWORD buffer[1];
} T_TTOS_MsgqBufControl;

/* 消息队列信息结构 */
typedef struct
{
    /*[out]: 消息队列名字 */
    UINT8 name[TTOS_OBJECT_NAME_LENGTH + 1];

    /*[out]: 一条消息的最大长度 */
    T_UWORD maxMsgSize;

    /*[out]: 允许的最大消息个数 */
    T_UWORD maxPendingMsg;

    /*[out]:当前未决消息数量 */
    T_UWORD pendingNumber;

    /*[out]: 等待在指定消息队列上的任务的数量 */
    T_UWORD numbeOfWaited;

    /*[in]:
     * 等待在指定消息队列上的任务列表存放缓冲区地址,用户需保证该地址空间的有效性
     */
    TASK_ID *waitedTaskIDArea;

    /*[in]:
     * 等待在指定消息队列上的任务列表存放缓冲区最大存放数目,用户需保证该空间的大小足够存放缓冲区最大存放数目
     */
    T_UWORD waitedTaskIDAreaSize;

    /*[in]: 当前未决消息列表存放缓冲区地址,用户需保证该地址空间的有效性 */
    T_VOID *pendingArea;

    /*[in]: 当前未决消息列表存放缓冲区最大存放信息大小
     * ，单位是字节,用户需保证该空间的大小足够存放缓冲区最大存放信息*/
    T_UWORD pendingAreaSize;
} T_TTOS_MsgqInfo;

typedef struct T_TTOS_MsgqControlBlock_Struct *MSGQ_ID;

typedef struct
{
    long mq_flags;
    long mq_maxmsg;
    long mq_msgsize;
    long mq_curmsgs;
} T_TTOS_MsgqAttr;

typedef void (*MSGQ_NOTIFY_HANDLER)(void *argument);

struct mq_control
{
    MSGQ_ID msgqID;                  /* 核心消息队列ID */
    unsigned long notify_tid;        /* 消息通知的线程ID */
    struct sigevent notify_sigevent; /* 消息通知信号事件 */
};

/*消息队列控制块*/
typedef struct T_TTOS_MsgqControlBlock_Struct
{
    /* 管理对象 */
    T_TTOS_ObjectCore objCore;

    /* 消息队列ID */
    MSGQ_ID msgqId;

    /* 消息队列的任务等待队列属性，队列排队类型优先级方式或者先进先出方式 */
    T_TTOS_Attribute attributeSet;

    /* 允许的最大消息个数 */
    T_UWORD maxPendingMsg;

    /* 允许的一个消息的最大长度 */
    T_UWORD maxMsgSize;

    /* 等待被接收的消息数量 */
    T_UWORD pendingCount;

    /* 消息优先级位图 */
    T_UWORD msgPriorityBitMap;

    /* 等待被接收的消息链表 */
    struct list_head pendingMsgQueue[TTOS_MSGQ_PRIORITY_NUMBER];

    /* 消息队列的消息块起始地址 */
    T_TTOS_MsgqBufControl *msgBuffers;

    /* 空闲消息块队列 */
    struct list_head inactiveMsgQueue;

    /* 发送等待的任务队列 */
    T_TTOS_Task_Queue_Control sendWaitQueue;

    /* 接收等待的任务队列 */
    T_TTOS_Task_Queue_Control receiveWaitQueue;

    /* 消息队列是否禁止删除的，消息拷贝过程中，是不允许被删除的 */
    T_UWORD disableDeleteFlag;

    /* 保存musl传递下来的attr */
    T_TTOS_MsgqAttr attr;

    struct mq_control mqc;

    /* 消息到达信号通知处理函数 */
    MSGQ_NOTIFY_HANDLER notifyHandler;

    /* 消息到达信号通知参数 */
    void *notifyArgument;

} T_TTOS_MsgqControlBlock;

/* 消息队列配置结构 */
typedef struct
{
    /*[in]: 消息队列的名字，最多为32个字符(不包含串尾结束符) */
    T_UBYTE cfgMsgqName[TTOS_OBJECT_NAME_LENGTH + 1];

    /*[in]:
     * 消息队列的任务等待队列属性，队列排队类型优先级方式或者先进先出方式 */
    T_TTOS_Attribute attributeSet;

    /*[in]: 一条消息的最大长度 */
    T_UWORD maxMsgSize;

    /*[in]:最大消息个数 */
    T_UWORD maxMsgqNumber;

    /*[in]:对象配置数据结构版本号 */
    T_UBYTE objVersion[TTOS_OBJECT_VERSION_LENGTH + 1];
} T_TTOS_ConfigMsgq;

#endif

/* 信号量配置结构 */
typedef struct
{
    /*[in]: 信号量的名字，最多为32个字符(不包含串尾结束符) */
    T_UBYTE cfgSemaName[TTOS_OBJECT_NAME_LENGTH + 1];

    /*[in]: 信号量初始值，对于互斥信号量初始值只能为0或1 */
    T_UWORD initValue;

    /*
     *[in]:信号量属性， TTOS_MUTEX_SEMAPHORE表示互斥信号量
     *TTOS_COUNTING_SEMAPHORE表示计数信号量
     */
    T_TTOS_Attribute attributeSet;

    /*[in]: 优先级天花板的优先级，该参数只对互斥信号量有效 */
    T_UWORD priorityCeiling;

    /*[in]:对象配置数据结构版本号 */
    T_UBYTE objVersion[TTOS_OBJECT_VERSION_LENGTH + 1];

} T_TTOS_ConfigSema;

/* 定时器配置结构 */
typedef struct
{
    /*[in]: 定时器的名字，最多为32个字符(不包含串尾结束符) */
    T_UBYTE cfgTimerName[TTOS_OBJECT_NAME_LENGTH + 1];

    /*[in]:
     * 定时器初始化时是否自动启动，TRUE表示自动启动，FALSE表示不自动启动 */
    T_UBYTE autoStarted;

    /*[in]: 定时器首次被触发的时间间隔，单位为tick */
    T_UWORD ticks;

    /*[in]:定时器的重复触发次数，0表示单次触发，1表示触发两次，依次类推，0xFFFFFFFF表示永久触发*/
    T_UWORD repeatCount;

    /*[in]:定时器处理函数的参数，用户需保证该地址空间的合法性 */
    T_VOID *argument;

    /*[in]:定时器被触发时的处理函数，用户需保证该地址空间的合法性 */
    T_TTOS_TimerRoutine handler;

    /*[in]:对象配置数据结构版本号 */
    T_UBYTE objVersion[TTOS_OBJECT_VERSION_LENGTH + 1];

} T_TTOS_ConfigTimer;

/* DeltaTT基本配置表 */
typedef struct
{
    /* 任务对象的配置结构指针 */
    T_TTOS_ConfigTask *taskConfig;

    /* 任务的控制结构指针 */
    T_TTOS_TaskControlBlock *taskCB;

    /*
     *所有任务的运行栈指针 ，根据任务运行栈的大小依次分配每个任务的
     *运行栈指针 ，此空间不能小于所有任务运行栈大小的总和
     */
    T_UBYTE *taskStack;

    /* 配置的任务个数
     * ，每个任务的运行栈大小总和不能大于taskStack定义的所有任务运行栈大小*/
    T_UWORD configTaskNumber;

    /* 用户可以动态创建的任务个数 */
    T_UWORD createTaskMaxNumber;

    /* 信号量对象的配置结构指针 */
    T_TTOS_ConfigSema *semaConfig;

    /* 信号量对象的控制结构指针 */
    T_TTOS_SemaControlBlock *semaCB;

    /* 配置的信号量个数*/
    T_UWORD configSemaNumber;

    /* 用户可以动态创建的信号量个数 */
    T_UWORD createSemaMaxNumber;

#ifdef TTOS_MSGQ
    /* 消息队列对象的配置结构指针 */
    T_TTOS_ConfigMsgq *msgqConfig;

    /* 消息队列对象的控制结构指针 */
    T_TTOS_MsgqControlBlock *msgqCB;

    /* 静态配置的消息队列个数 */
    T_UWORD configMsgqNumber;

    /* 用户可以动态创建的消息队列个数 */
    T_UWORD createMsgqMaxNumber;
#endif

    /* 定时器对象的配置结构指针 */
    T_TTOS_ConfigTimer *timerConfig;

    /* 定时器对象的控制结构指针 */
    T_TTOS_TimerControlBlock *timerCB;

    /* 配置的定时器个数*/
    T_UWORD configTimerNumber;

    /* 用户可以动态创建的定时器个数 */
    T_UWORD createTimerMaxNumber;

    /*
     *配置是否填充任务栈 ，TRUE表示需要，DeltaTT初始化时会使用特殊的字符填充
     *栈空间，便于可以查询栈使用的情况；FALSE表示不需要
     */
    T_BOOL taskStackFill;

    /*
     *TTOS事件记录掩码
     *，配置DeltaTT运行过程中可记录的事件，现在可记录的事件为任务切换事件，
     *任务切换事件掩码为TTOS_RECORD_TASK_SWITCH_MASK
     */
    T_UWORD ttosEveRecordMask;

} T_TTOS_ConfigTable;

/* 值或信息不可用 */

#define TTOS_INVALID_VALUE UL(-1)

/* 栈信息结构 */
typedef struct
{
    /*[out]: 任务运行栈基地址 */
    T_UBYTE *stackBase;

    /*[out]: 任务运行栈底地址 */
    T_UBYTE *stackBottom;

    /*[out]: 任务运行栈的起始地址 */
    T_UBYTE *stackStart;

    /*[out]: 任务运行栈的大小 */
    T_ULONG stackSize;

    /*[out]: 任务的当前运行栈 */
    T_UBYTE *currentStack;

    /*[out]: 任务运行时使用的最大栈的大小 */
    T_ULONG stackMaxUseSize;

    /*[out]: 栈是否上溢 */
    T_BOOL isStackOverflowed;

    /*[out]: 栈是否下溢 */
    T_BOOL isStackUnderflowed;

    /*[out]: 是否在TTOS_ExitTaskHook()中释放任务栈*/
    T_BOOL isFreeStack;

} T_TTOS_TaskStackInformation;

struct period_param
{
    T_UWORD periodTime;
    T_UWORD durationTime;
    T_UWORD delayTime;
    T_UWORD autoStarted;
};
/************************接口声明******************************/
T_VOID TTOS_StartOS(T_TTOS_ConfigTable *ttosConfigureTable, T_UWORD mode);
T_VOID ttosAPEntry(T_VOID);
T_TTOS_ReturnCode TTOS_CreateTask(T_TTOS_ConfigTask *taskParam, TASK_ID *taskID);
T_TTOS_ReturnCode TTOS_DeleteTask(TASK_ID taskID);
T_VOID TTOS_DisablePreempt(T_VOID);
T_VOID TTOS_EnablePreempt(T_VOID);
T_TTOS_ReturnCode TTOS_ActiveTask(TASK_ID taskID);
T_ULONG ttosGetPageSize(T_VOID);
/**
 * @brief
 *    创建非周期任务
 * @param[in]: taskName:待创建的任务名
 * @param[in]: taskPriority: 待创建的任务优先级
 * @param[in]: autoStarted:待创建的任务是否自启动
 * @param[in]: preempted:待创建的任务是否可抢占
 * @param[in]: entry:待创建的任务入口，用户需保证该地址空间的有效性。
 * @param[in]: arg:待创建的任务运行时的参数， 用户需保证该地址空间的有效性。
 * @param[in]: stackSize:待创建的任务栈大小。
 * @param[out]: taskID: 创建的任务标识符。
 * @return
 *       TTOS_INVALID_ADDRESS: taskID或任务入口地址、或栈地址为NULL。
 *       TTOS_INVALID_NUMBER: 任务优先级不是0-31。
 *       TTOS_INVALID_SIZE: 栈大小小于4KB或周期任务的执行时间大于周期时间。
 *       TTOS_TOO_MANY:
 * 系统中正在使用的任务数已达到用户配置的最大任务数(静态 配置的任务数+
 * 可创建的任务数)。 TTOS_UNSATISFIED:
 * 从中断中分配任务栈;系统不能成功分配的任务需要的栈空间；
 *                                     分配任务对象失败。
 *       TTOS_INVALID_NAME:
 * 对象名name字符串中存在ASCII取值范围不在[32,126]之间的字符 或name为空串。
 *       TTOS_OK: 成功创建任务。
 * @implements:
 */
T_INLINE T_TTOS_ReturnCode TTOS_CreateTaskEx(T_UBYTE *taskName, T_UBYTE taskPriority,
                                             T_UBYTE autoStarted, T_UBYTE preempted,
                                             T_TTOS_TaskRoutine entry, T_VOID *arg,
                                             T_UWORD stackSize, TASK_ID *taskID)
{
    T_TTOS_ReturnCode ret;
    T_TTOS_ConfigTask taskParam = {"\0",
                                   taskPriority,
                                   FALSE,
                                   preempted,
                                   entry,
                                   arg,
                                   TTOS_DEFAULT_TASK_SLICE_SIZE,
                                   stackSize + ttosGetPageSize(),
                                   TTOS_SCHED_NONPERIOD,
                                   0,
                                   0,
                                   0,
                                   0,
                                   TTOS_OBJ_CONFIG_CURRENT_VERSION,
                                   TRUE,
                                   FALSE};

    /* 任务名为空 */
    if (NULL == taskName)
    {
        ttosCopyName((T_UBYTE *)TTOS_DEFAULT_TASK_NAME, taskParam.cfgTaskName);
    }
    else
    {
        ttosCopyName(taskName, taskParam.cfgTaskName);
    }

    /* 任务栈小于TTOS_MINIMUM_STACK_SIZE */
    if (stackSize < TTOS_MINIMUM_STACK_SIZE)
    {
        return TTOS_INVALID_SIZE;
    }

    /* 任务栈大小超出T_UWORD_MAX*/
    if (T_UWORD_MAX - stackSize <= TTOS_MINIMUM_STACK_SIZE)
    {
        return TTOS_UNSATISFIED;
    }

    /* 动态分配任务栈 */
    taskParam.taskStack = (T_UBYTE *)memalign(ttosGetPageSize(), stackSize + ttosGetPageSize());

    /* 任务栈起始地址为空 */
    if (NULL == taskParam.taskStack)
    {
        ret = TTOS_UNSATISFIED;
        return ret;
    }

    ttosSetFreeStackRoutine(free);

    ret = TTOS_CreateTask(&taskParam, taskID);

    if (TTOS_OK != ret)
    {
        free(taskParam.taskStack);
        return ret;
    }

    if (TRUE == autoStarted)
    {
        /*
         *在最后启动避免还未设置任务的栈保护处时，此任务就已经运行，此任务运行
         *时若存在栈溢出的情况也不能得到发现。
         */
        TTOS_ActiveTask(*taskID);
    }

    return TTOS_OK;
}

/**
 * @brief
 *       创建周期任务。
 * @param[in]: taskName:待创建的任务名
 * @param[in]: taskPriority: 待创建的任务优先级
 * @param[in]:autoStarted:待创建的任务是否自启动
 * @param[in]:preempted:待创建的任务是否可抢占
 * @param[in]:entry:待创建的任务入口，用户需保证该地址空间的有效性。
 * @param[in]:arg:待创建的任务运行时的参数， 用户需保证该地址空间的有效性。
 * @param[in]:stackSize:待创建的任务栈大小。
 * @param[in]:periodTime:待创建的任务周期。
 * @param[in]:durationTime:待创建的任务截止期。
 * @param[in]:delayTime:待创建的任务延时启动时间。
 * @param[out]: taskID: 创建的任务标识符。
 * @return
 *       TTOS_INVALID_ADDRESS: taskID或任务入口地址、或栈地址为NULL。
 *       TTOS_INVALID_NUMBER: 任务优先级不是0-31。
 *       TTOS_INVALID_SIZE: 栈大小小于4KB或周期任务的执行时间大于周期时间。
 *       TTOS_TOO_MANY:
 * 系统中正在使用的任务数已达到用户配置的最大任务数(静态 配置的任务数+
 * 可创建的任务数)。 TTOS_UNSATISFIED:
 * 从中断中分配任务栈;系统不能成功分配的任务需要的栈空间；
 *                                     分配任务对象失败。
 *       TTOS_INVALID_NAME:
 * 对象名name字符串中存在ASCII取值范围不在[32,126]之间的字符
 *                                    或name为空串或者空指针。
 *       TTOS_OK: 成功创建任务。
 * @implements:
 */
T_INLINE T_TTOS_ReturnCode TTOS_CreatePeriodTask(T_UBYTE *taskName, T_UBYTE taskPriority,
                                                 T_UBYTE autoStarted, T_UBYTE preempted,
                                                 T_TTOS_TaskRoutine entry, T_VOID *arg,
                                                 T_UWORD stackSize, T_UWORD periodTime,
                                                 T_UWORD durationTime, T_UWORD delayTime,
                                                 TASK_ID *taskID)
{
    T_TTOS_ReturnCode ret;

    T_TTOS_ConfigTask taskParam = {"\0",
                                   taskPriority,
                                   autoStarted,
                                   preempted,
                                   entry,
                                   arg,
                                   TTOS_DEFAULT_TASK_SLICE_SIZE,
                                   stackSize + ttosGetPageSize(),
                                   TTOS_SCHED_PERIOD,
                                   periodTime,
                                   durationTime,
                                   delayTime,
                                   0,
                                   TTOS_OBJ_CONFIG_CURRENT_VERSION,
                                   TRUE};

    /* 任务名为空 */
    if (taskName == NULL)
    {
        ttosCopyName((T_UBYTE *)TTOS_DEFAULT_TASK_NAME, taskParam.cfgTaskName);
    }
    else
    {
        ttosCopyName(taskName, taskParam.cfgTaskName);
    }

    /* 任务栈小于TTOS_MINIMUM_STACK_SIZE */
    if (stackSize < TTOS_MINIMUM_STACK_SIZE)
    {
        return TTOS_INVALID_SIZE;
    }

    /* 任务栈大小超出T_UWORD_MAX*/
    if (T_UWORD_MAX - stackSize <= TTOS_MINIMUM_STACK_SIZE)
    {
        return TTOS_UNSATISFIED;
    }

    /* 动态分配任务栈 */
    taskParam.taskStack = (T_UBYTE *)memalign(ttosGetPageSize(), stackSize + ttosGetPageSize());

    /* 任务栈起始地址为空 */
    if (NULL == taskParam.taskStack)
    {
        ret = TTOS_UNSATISFIED;
        return ret;
    }

    ttosSetFreeStackRoutine(free);

    ret = TTOS_CreateTask(&taskParam, taskID);

    if (TTOS_OK != ret)
    {
        free(taskParam.taskStack);
        return ret;
    }

    return TTOS_OK;
}

/**
 * @brief
 *       初始化任务参数。初始化任务的名字、可抢占、时间片大小、配置参数数据结构的版本号
 *为默认值，任务类型设置为非周期任务。
 * @param[in]: taskParam: 任务参数
 * @return
 *      TTOS_INVALID_ADDRESS:参数地址无效
 *      TTOS_OK:初始化任务参数成功
 * @implements:
 */
T_INLINE T_TTOS_ReturnCode TTOS_InitTaskConfig(T_TTOS_ConfigTask *taskParam)
{
    /* 参数地址为空 */
    if (NULL == taskParam)
    {
        return TTOS_INVALID_ADDRESS;
    }

    /* 将参数清零 */
    memset((T_VOID *)taskParam, 0, sizeof(*taskParam));

    /* 设置任务为可抢占 */
    taskParam->preempted = TRUE;

    /* 设置任务时间片为TTOS_DEFAULT_TASK_SLICE_SIZE */
    taskParam->tickSliceSize = TTOS_DEFAULT_TASK_SLICE_SIZE;

    /* 设置任务不需要释放栈 */
    taskParam->isFreeStack = FALSE;

    /* 设置任务类型为TTOS_SCHED_NONPERIOD */
    taskParam->taskType = TTOS_SCHED_NONPERIOD;

    /* 设置任务名为DEFAULT_TASK_NAME */
    ttosCopyName((T_UBYTE *)TTOS_DEFAULT_TASK_NAME, taskParam->cfgTaskName);

    /* 设置任务版本号为TTOS_OBJ_CONFIG_CURRENT_VERSION */
    ttosCopyVersion((T_UBYTE *)TTOS_OBJ_CONFIG_CURRENT_VERSION, taskParam->objVersion);

    return TTOS_OK;
}

T_TTOS_ReturnCode TTOS_StopTask(TASK_ID taskID);
T_TTOS_ReturnCode TTOS_SleepTask(T_UWORD ticks);
T_VOID TTOS_Modify_SleepTask(TASK_ID taskID, T_UWORD ticks);

T_TTOS_ReturnCode TTOS_SuspendTask(TASK_ID taskID);
T_TTOS_ReturnCode TTOS_ResumeTask(TASK_ID taskID);
T_TTOS_ReturnCode TTOS_ResetTask(TASK_ID taskID);
T_TTOS_ReturnCode TTOS_GetTaskIdList(TASK_ID idList[], T_UWORD maxIdNum);
T_TTOS_ReturnCode TTOS_GetTaskIdList(TASK_ID idList[], T_UWORD maxIdNum);
T_TTOS_ReturnCode TTOS_GetTaskInfo(TASK_ID taskID, T_TTOS_TaskInfo *info);
T_TTOS_ReturnCode TTOS_GetTaskContext(TASK_ID taskID, T_TBSP_TaskContext *taskContext);
T_TTOS_ReturnCode TTOS_GetTaskName(TASK_ID taskID, T_UBYTE *taskName);
T_TTOS_ReturnCode TTOS_SetTaskName(TASK_ID taskID, T_UBYTE *taskName);
T_TTOS_ReturnCode TTOS_GetTaskID(T_UBYTE *taskName, TASK_ID *taskID);
T_TTOS_ReturnCode TTOS_GetTaskPriority(TASK_ID taskID, T_UBYTE *taskPriority);
T_TTOS_ReturnCode TTOS_SetTaskPriority(T_TTOS_TaskControlBlock *theTask, T_UBYTE newPriority,
                                       T_BOOL prependIt);
T_TTOS_ReturnCode TTOS_GetTaskOriginPriority(TASK_ID taskID, T_UBYTE *taskPriority);
T_TTOS_ReturnCode TTOS_GetTaskStackInfo(TASK_ID taskID, T_TTOS_TaskStackInformation *taskStackInfo);
T_TTOS_ReturnCode TTOS_GetTaskCount(T_UWORD *taskCount);
T_TTOS_ReturnCode TTOS_WaitPeriod(T_VOID);
T_TTOS_ReturnCode TTOS_ReplenishTask(TASK_ID taskID, T_UWORD delay_time);
T_TTOS_ReturnCode TTOS_SetDelayTime(TASK_ID taskID, T_UWORD delay_time);

T_TTOS_ReturnCode TTOS_CreateSema(T_TTOS_ConfigSema *semaParam, SEMA_ID *semaID);
T_TTOS_ReturnCode TTOS_DeleteSema(SEMA_ID semaID);
T_TTOS_ReturnCode TTOS_ReleaseSema(SEMA_ID semaID);
T_TTOS_ReturnCode TTOS_ResetSema(T_WORD count, SEMA_ID semaID);
T_TTOS_ReturnCode TTOS_ObtainSemaWithInterruptFlags(SEMA_ID semaID, T_UWORD ticks,
                                                    T_UWORD is_interruptable);
T_TTOS_ReturnCode TTOS_ReleaseSemaReturnTask(SEMA_ID semaID, TASK_ID *wakedup_task);
/**
 * @brief
 *       创建匿名计数信号量。
 * @param[in]: initValue:待创建的计数信号量初始值。
 * @param[out]: semaID: 创建的信号量标识符
 * @return
 *       TTOS_CALLED_FROM_ISR: 从中断中调用。
 *       TTOS_INVALID_ADDRESS: semaID为空。
 *       TTOS_UNSATISFIED:
 * 系统中正在使用的信号量数已达到用户配置的最大信号量数(静态
 *                                     配置的信号量数+可创建的信号量数)；
 *                                     分配信号量对象失败。
 *       TTOS_OK: 成功创建信号量。
 * @implements:
 */
T_INLINE T_TTOS_ReturnCode TTOS_CreateSemaEx(T_UWORD initValue, SEMA_ID *semaID)
{
    T_TTOS_ReturnCode ret;

    /* 设置信号量参数 */
    T_TTOS_ConfigSema semaParam = {TTOS_DEFAULT_SEMA_NAME, initValue, TTOS_COUNTING_SEMAPHORE, 0,
                                   TTOS_OBJ_CONFIG_CURRENT_VERSION};

    /* 创建信号量 */
    ret = TTOS_CreateSema(&semaParam, semaID);

    return ret;
}

T_INLINE T_TTOS_ReturnCode TTOS_ObtainSema(SEMA_ID semaID, T_UWORD ticks)
{
    return TTOS_ObtainSemaWithInterruptFlags(semaID, ticks, TRUE);
}

T_INLINE T_TTOS_ReturnCode TTOS_ObtainSemaUninterruptable(SEMA_ID semaID, T_UWORD ticks)
{
    return TTOS_ObtainSemaWithInterruptFlags(semaID, ticks, FALSE);
}

T_INLINE T_TTOS_ReturnCode TTOS_ResetMutex(MUTEX_ID mutexID)
{
    T_TTOS_ReturnCode ret;

    if (TRUE != ttosSemaIsMutex(mutexID))
    {
        return TTOS_INVALID_ID;
    }
    mutexID->semHolder = TTOS_NULL_OBJECT_ID;
    mutexID->nestCount = 0;

    /* 删除互斥锁 */
    ret = TTOS_ResetSema(1, mutexID);

    return ret;
}

/**
 * @brief
 *       创建互斥信号量。
 * @param[in]: initValue: 互斥信号量初始值(0表示锁定状态，1表示解锁状态)
 * @param[in]: priorityCeiling: 互斥信号量优先级天花板
 * @param[in/out]: mutexID: 创建的信号量标识符
 * @return
 *       TTOS_CALLED_FROM_ISR: 从中断中调用。
 *       TTOS_INVALID_ADDRESS: semaParam或semaID为空。
 *       TTOS_INVAILD_VERSION: 版本不一致。
 *       TTOS_RESOURCE_IN_USE: semaID指向的资源已被创建。
 *       TTOS_UNSATISFIED:
 * 系统中正在使用的信号量数已达到用户配置的最大信号量数(静态
 *                                     配置的信号量数+可创建的信号量数)；
 *                                     分配信号量对象失败。
 *       TTOS_INVALID_NAME:
 * 对象名name字符串中存在ASCII取值范围不在[32,126]之间的字符
 *                                    或name为空串或者空指针。
 *       TTOS_INVALID_NUMBER：互斥信号量初始值大于1。
 *       TTOS_INVALID_PRIORITY：天花板优先级小于任务的最低优先级
 *       TTOS_INVALID_ATTRIBUTE：属性不为 TTOS_COUNTING_SEMAPHORE、
 * TTOS_BINARY_SEMAPHORE 和 TTOS_MUTEX_SEMAPHOR 或者 排队策略错误
 *       TTOS_MUTEX_CEILING_VIOLATED:
 * 创建锁定状态的互斥锁时违反优先级天花板策略， 任务优先级高于天花板优先级。
 *       TTOS_OK: 成功创建信号量。
 * @implements:
 */
T_INLINE T_TTOS_ReturnCode TTOS_CreateMutex(T_UWORD initValue, T_UWORD priorityCeiling,
                                            MUTEX_ID *mutexID)
{
    T_TTOS_ReturnCode ret;

    /* 设置信号量参数 */
    T_TTOS_ConfigSema mutexParam = {TTOS_DEFAULT_SEMA_NAME, initValue,
                                    TTOS_MUTEX_SEMAPHORE | TTOS_MUTEX_WAIT_PRIORITY_INHERIT,
                                    priorityCeiling, TTOS_OBJ_CONFIG_CURRENT_VERSION};

    /* 创建信号量 */
    ret = TTOS_CreateSema(&mutexParam, mutexID);
    return ret;
}

/**
 * @brief
 *       删除匿名互斥锁。
 * @param[in]  mutexID: 要删除的互斥锁标识符
 * @return
 *       TTOS_CALLED_FROM_ISR: 从中断中调用。
 *       TTOS_INVALID_ID: 无效的互斥锁标识符。
 *       TTOS_OK: 成功删除指定互斥锁。
 * @implements:
 */
T_INLINE T_TTOS_ReturnCode TTOS_DeleteMutex(MUTEX_ID mutexID)
{
    T_TTOS_ReturnCode ret;

    if (TRUE != ttosSemaIsMutex(mutexID))
    {
        return TTOS_INVALID_ID;
    }
    /* 删除互斥锁 */
    ret = TTOS_DeleteSema(mutexID);

    return ret;
}

/**
 * @brief:
 *    获取指定互斥锁，当不能立即获得互斥锁时，根据指定的等待方式等待互斥锁。
 * @param[in]: mutexID:互斥锁的ID
 * @param[in]: ticks:
 * 任务等待时间，单位为tick。等待时间为0表示任务不等待互斥锁；
 *                    <ticks>为TTOS_MUTEX_WAIT_FOREVER表示任务永久等待互斥锁。
 * @return:
 *    TTOS_CALLED_FROM_ISR：在虚拟中断处理程序中执行此接口。
 *    TTOS_INVALID_USER：当前任务是IDLE任务。
 *    TTOS_INVALID_ID： 无效的互斥锁标识符。
 *    TTOS_UNSATISFIED：
 * 当前信号量为互斥锁且当前任务优先级大于天花板优先级或互斥锁的计数值为0且任务等待时间为0。
 *    TTOS_TIMEOUT:
 * 互斥锁的计数值为0且指定等待时间到后任务还没有获得互斥锁。
 *    TTOS_OK：获取互斥锁成功。
 * @implements:
 */
T_INLINE T_TTOS_ReturnCode TTOS_ObtainMutex(MUTEX_ID mutexID, T_UWORD ticks)
{
    T_TTOS_ReturnCode ret;

    if (TRUE != ttosSemaIsMutex(mutexID))
    {
        return TTOS_INVALID_ID;
    }

    /* 获取互斥锁 */
    ret = TTOS_ObtainSemaUninterruptable(mutexID, ticks);

    return ret;
}

T_INLINE T_TTOS_ReturnCode TTOS_ObtainMutexInterruptible(MUTEX_ID mutexID, T_UWORD ticks)
{
    T_TTOS_ReturnCode ret;

    if (TRUE != ttosSemaIsMutex(mutexID))
    {
        return TTOS_INVALID_ID;
    }

    /* 获取互斥锁 */
    ret = TTOS_ObtainSema(mutexID, ticks);

    return ret;
}

/**
 * @brief:
 *    释放指定的互斥锁。任务在释放互斥锁时会去检测是否有任务因为该互斥锁而阻塞，
 *    如果有就会将该任务的优先级提升到天花板的优先级。
 * @param[in]: mutexID: 互斥锁的ID
 * @return:
 *    TTOS_CALLED_FROM_ISR：互斥锁在虚拟中断处理程序中执行此接口。
 *    TTOS_NOT_OWNER_OF_RESOURCE：当前任务不是互斥锁的拥有者。
 *         TTOS_INVALID_ID： 无效的互斥锁标识符。
 *         TTOS_OK：释放互斥锁成功。
 * @implements:
 */
T_INLINE T_TTOS_ReturnCode TTOS_ReleaseMutexAndGetHolder(MUTEX_ID mutexID, TASK_ID *task)
{
    T_TTOS_ReturnCode ret;

    if (TRUE != ttosSemaIsMutex(mutexID))
    {
        return TTOS_INVALID_ID;
    }

    /* 释放互斥锁 */
    ret = TTOS_ReleaseSemaReturnTask(mutexID, task);

    return ret;
}

T_INLINE T_TTOS_ReturnCode TTOS_ReleaseMutex(MUTEX_ID mutexID)
{
    return TTOS_ReleaseMutexAndGetHolder(mutexID, NULL);
}

/**
 * @brief
 *       初始化信号量参数。初始化信号量的名字、配置参数数据结构的版本号和
 *       信号量类型为默认值，默认信号量类型为计数信号量。
 * @param[in]: semaParam: 信号量参数
 * @return
 *          TTOS_INVALID_ADDRESS:参数地址无效
 *          TTOS_OK:初始化信号量参数成功
 * @implements:
 */
T_INLINE T_TTOS_ReturnCode TTOS_InitSemaConfig(T_TTOS_ConfigSema *semaParam)
{
    /* 参数地址为空 */
    if (NULL == semaParam)
    {
        return TTOS_INVALID_ADDRESS;
    }

    /* 将参数清零 */
    memset((T_VOID *)semaParam, 0, sizeof(*semaParam));

    /* 设置信号量类型为计数信号量 */
    semaParam->attributeSet = TTOS_COUNTING_SEMAPHORE;

    /* 设置信号量默认名为DEFAULT_SEMA_NAME */
    ttosCopyName((T_UBYTE *)TTOS_DEFAULT_SEMA_NAME, semaParam->cfgSemaName);

    /* 设置任务版本号为TTOS_OBJ_CONFIG_CURRENT_VERSION */
    ttosCopyVersion((T_UBYTE *)TTOS_OBJ_CONFIG_CURRENT_VERSION, semaParam->objVersion);

    return TTOS_OK;
}

T_TTOS_ReturnCode TTOS_GetSemaID(T_UBYTE *semaName, SEMA_ID *semaID);
T_TTOS_ReturnCode TTOS_GetSemaName(SEMA_ID semaID, T_UBYTE *semaName);
T_TTOS_ReturnCode TTOS_CreateTimer(T_TTOS_ConfigTimer *timerParam, TIMER_ID *timerID);

/**
 * @brief
 *       创建可匿名的定时器。
 * @param[in]:autoStarted:待创建的定时器是否自动启动，TRUE表示自动启动，FALSE表示不自动启动
 * @param[in]:ticks:待创建的定时器被触发的时间间隔
 * @param[in]:repeatCount:定时器的重复触发次数，0表示单次触发，1表示触发两次，依次类推，0xFFFFFFFF表示永久触发
 * @param[in]:argument:定时器处理函数的参数，用户需保证该地址空间的合法性
 * @param[in]:handler:待创建的定时器处理函数
 * @param[out]: timerID: 创建的定时器标识符
 * @return
 *       TTOS_CALLED_FROM_ISR: 从中断中调用。
 *       TTOS_INVALID_ADDRESS:
 * timerParam或timerID或自启动定时器处理函数入口地址为NULL。
 *       TTOS_INVAILD_VERSION:版本不一致。
 *       TTOS_INVALID_NUMBER: 自启动定时器定时器被触发的时间间隔为0。
 *       TTOS_TOO_MANY:
 * 系统中正在使用的定时器数已达到用户配置的最大定时器数(静态
 *                                  配置的定时器数+可创建的定时器数)。
 *       TTOS_UNSATISFIED: 分配定时器对象失败。
 *       TTOS_INVALID_NAME:
 * 对象名name字符串中存在ASCII取值范围不在[32,126]之间的字符
 *                                    或name为空串或者空指针。
 *       TTOS_OK: 成功创建定时器。
 * @implements:
 */
T_INLINE T_TTOS_ReturnCode TTOS_CreateTimerEx(T_UBYTE autoStarted, T_UWORD ticks,
                                              T_UWORD repeatCount, T_VOID *argument,
                                              T_TTOS_TimerRoutine handler, TIMER_ID *timerID)
{
    T_TTOS_ReturnCode ret;

    /* 初始化定时器参数 */
    T_TTOS_ConfigTimer timerParam = {
        TTOS_DEFAULT_TIMER_NAME,        autoStarted, ticks, repeatCount, argument, handler,
        TTOS_OBJ_CONFIG_CURRENT_VERSION};

    /* 创建定时器 */
    ret = TTOS_CreateTimer(&timerParam, timerID);

    return ret;
}

/**
 * @brief
 *       初始化定时器参数。初始化定时器的名字、配置参数数据结构的版本号
 *为默认值。
 * @param[in]: timerParam: 定时器参数
 * @return
 *       TTOS_INVALID_ADDRESS:参数地址无效
 *       T_TTOS:初始化定时器参数成功
 * @implements:
 */
T_INLINE T_TTOS_ReturnCode TTOS_InitTimerConfig(T_TTOS_ConfigTimer *timerParam)
{
    /* 参数地址为空 */
    if (timerParam == NULL)
    {
        return TTOS_INVALID_ADDRESS;
    }

    /* 将参数清零 */
    memset((T_VOID *)timerParam, 0, sizeof(*timerParam));

    /* 设置定时器默认名为DEFAULT_TIMER_NAME */
    ttosCopyName((T_UBYTE *)TTOS_DEFAULT_TIMER_NAME, timerParam->cfgTimerName);

    /* 设置定时器默认版本号为TTOS_OBJ_CONFIG_CURRENT_VERSION */
    ttosCopyVersion((T_UBYTE *)TTOS_OBJ_CONFIG_CURRENT_VERSION, (T_UBYTE *)timerParam->objVersion);

    return TTOS_OK;
}

T_TTOS_ReturnCode TTOS_DeleteTimer(TIMER_ID timerID);
T_TTOS_ReturnCode TTOS_InstallTimer(TIMER_ID timerID, T_UWORD ticks, T_TTOS_TimerRoutine handler,
                                    T_VOID *argument, T_UWORD repeatCount);

T_TTOS_ReturnCode TTOS_ActiveTimer(TIMER_ID timerID);
T_TTOS_ReturnCode TTOS_StopTimer(TIMER_ID timerID);
T_TTOS_ReturnCode TTOS_GetTimerID(T_UBYTE *timerName, TIMER_ID *timerID);
T_TTOS_ReturnCode TTOS_GetTimerName(TIMER_ID timerID, T_UBYTE *timerName);
T_TTOS_ReturnCode TTOS_GetTimerState(TIMER_ID timerID, T_TTOS_TimerState *state);
T_TTOS_ReturnCode TTOS_GetTimerInfo(TIMER_ID timerID, T_TTOS_TimerInfo *timerInfo);
T_TTOS_ReturnCode TTOS_NotifyHM(TASK_ID taskID, T_TTOS_HMAction action);
T_VOID TTOS_IsDisableKernelHeap(BOOL isDisable);
T_TTOS_ReturnCode TTOS_GetTimerCount(T_UWORD *timerCount);
T_TTOS_ReturnCode TTOS_GetTimerIdList(TIMER_ID idList[], T_UWORD maxIdNum);

TASK_ID TTOS_GetRunningTask();

T_VOID TTOS_IdleHook(T_VOID);
T_VOID TTOS_ScheduleEnterHook(T_VOID);
T_VOID TTOS_ScheduleLeaveHook(T_VOID);
T_VOID TTOS_CandidateEnterHook(T_VOID);
T_VOID TTOS_CandidateLeaveHook(T_VOID);
T_VOID TTOS_StartHook(T_UWORD mode);
T_VOID TTOS_TaskStartHook(TASK_ID taskID);
T_VOID TTOS_EnterTaskHook(TASK_ID taskID);
T_VOID TTOS_LeaveTaskHook(TASK_ID taskID);
T_VOID TTOS_TaskEnterKernelHook(TASK_ID taskID);
T_VOID TTOS_TaskEnterUserHook(TASK_ID taskID);
T_VOID TTOS_ShutdownHook(T_TTOS_HMAction action);
T_VOID TTOS_HMHook(TASK_ID taskID, T_TTOS_HMAction action);
T_VOID TTOS_ExpireTaskHook(TASK_ID taskID);
T_VOID TTOS_ExitTaskHook(TASK_ID taskID);
T_VOID TTOS_InitErrorHook(T_CONST T_CHAR *funName, T_WORD lineNum,
                          T_TTOS_Init_Eerros_List initErrorType);
T_VOID TTOS_InternalErrorHook(T_CONST T_CHAR *funName, T_WORD lineNum,
                              T_TTOS_Internal_Eerros_List initErrorType);

T_TTOS_ReturnCode TTOS_UninstallVintHandler(T_VMK_VintType vintType, T_UWORD vintNum);

T_TTOS_ReturnCode TTOS_InstallIntHandler(T_UBYTE intNum, T_TTOS_ISR_HANDLER handler,
                                         T_TTOS_ISR_HANDLER *oldHandler);
T_TTOS_ReturnCode TTOS_EnablePIC(T_UBYTE intNum);
T_TTOS_ReturnCode TTOS_DisablePIC(T_UBYTE intNum);
T_TTOS_ReturnCode TTOS_AllocVector(T_UWORD *vector);
T_TTOS_ReturnCode TTOS_MapExintToVint(T_UWORD vector, T_UWORD *vint);
T_UWORD intCpuLock(T_VOID);
T_VOID intCpuUnlock(T_ULONG key);

T_TTOS_ReturnCode TTOS_SetIsrStackProtect(T_UWORD guardSize);

#ifdef TTOS_EVENT
T_VOID ttos_init_event(T_TTOS_EventControl *event);
T_VOID ttos_deinit_event(T_TTOS_EventControl *event);
T_TTOS_EventControl *TTOS_CreateEvent(T_VOID);
T_VOID TTOS_DeleteEvent(T_TTOS_EventControl *event_cb);
T_TTOS_ReturnCode TTOS_ReceiveEvent(T_TTOS_EventControl *eventCB, T_TTOS_EventSet eventIn,
                                    T_TTOS_Option optionSet, T_UWORD ticks,
                                    T_TTOS_EventSet *eventOut);
T_TTOS_ReturnCode TTOS_SendEvent(T_TTOS_EventControl *eventCB, T_TTOS_EventSet eventIn);
T_TTOS_ReturnCode TTOS_GetEventInfo(TASK_ID taskID, T_TTOS_EventSet *condition,
                                    T_TTOS_Option *option, T_TTOS_EventSet *pending);
#endif /*TTOS_EVENT*/

#ifdef TTOS_MSGQ
T_TTOS_ReturnCode TTOS_CreateMsgq(T_TTOS_ConfigMsgq *msgqParam, MSGQ_ID *msgqID);

/**
 * @brief
 *       创建匿名消息队列。
 * @param[in] maxMsgSize:待创建的消息队列一条消息的最大长度
 * @param[in] maxMsgqNumber:待创建的消息队列最大消息个数
 * @param[out] msgqID: 创建的消息队列标识符
 * @return:
 *       TTOS_CALLED_FROM_ISR: 从中断中调用
 *       TTOS_INVALID_ADDRESS: msgqID为空
 *       TTOS_INVALID_NUMBER: maxMsgqNumber为0
 *       TTOS_INVALID_SIZE: maxMsgSize为0
 *       TTOS_UNSATISFIED:
 * 系统中正在使用的消息队列数已达到用户配置的最大消息队列数
 *                        (静态配置的消息队列数+ 可创建的消息队列数)；
 *                       系统不能成功分配的消息队列需要的空间；
 *                       分配消息队列对象失败。
 *       TTOS_OK: 成功创建消息队列
 * @implements
 */
T_INLINE T_TTOS_ReturnCode TTOS_CreateMsgqEx(T_UWORD maxMsgSize, T_UWORD maxMsgqNumber,
                                             MSGQ_ID *msgqID)
{
    T_TTOS_ReturnCode ret;

    /* 设置消息队列参数 */
    T_TTOS_ConfigMsgq msgqParam = {TTOS_DEFAULT_MSGQ_NAME, TTOS_FIFO, maxMsgSize, maxMsgqNumber,
                                   TTOS_OBJ_CONFIG_CURRENT_VERSION};

    /* 创建消息队列 */
    ret = TTOS_CreateMsgq(&msgqParam, msgqID);

    return ret;
}

/**
 * @brief
 *       初始化消息队列参数。初始化消息队列的名字、配置参数数据结构的版本号
 *为默认值。
 * @param[in]: msgqParam: 消息队列参数
 * @return
 *       TTOS_INVALID_ADDRESS:参数地址无效
 *       TTOS_OK:初始化消息队列参数成功
 * @implements:
 */
T_INLINE T_TTOS_ReturnCode TTOS_InitMsgqConfig(T_TTOS_ConfigMsgq *msgqParam)
{
    /* 参数地址为空 */
    if (msgqParam == NULL)
    {
        return TTOS_INVALID_ADDRESS;
    }

    /* 清除参数 */
    memset((T_VOID *)msgqParam, 0, sizeof(*msgqParam));

    /* 设置消息队列默认名字为DEFAULT_MSGQ_NAME*/
    ttosCopyName((T_UBYTE *)TTOS_DEFAULT_MSGQ_NAME, msgqParam->cfgMsgqName);

    /* 设置消息队列的默认版本号为TTOS_OBJ_CONFIG_CURRENT_VERSION */
    ttosCopyVersion((T_UBYTE *)TTOS_OBJ_CONFIG_CURRENT_VERSION, (T_UBYTE *)msgqParam->objVersion);

    return TTOS_OK;
}

T_TTOS_ReturnCode TTOS_DeleteMsgq(MSGQ_ID msgqID);
T_TTOS_ReturnCode TTOS_SendMsgq(MSGQ_ID msgqID, T_VOID *buffer, T_UWORD size, T_UWORD options,
                                T_UWORD ticks);
T_TTOS_ReturnCode TTOS_ReceiveMsgq(MSGQ_ID msgqID, T_VOID *buffer, T_UWORD *size, T_UWORD options,
                                   T_UWORD ticks);
T_TTOS_ReturnCode TTOS_GetMsgqID(T_UBYTE *msgqName, MSGQ_ID *msgqID);
T_TTOS_ReturnCode TTOS_GetMsgqClassInfo(T_UWORD *msgqCount, MSGQ_ID *msgqArea,
                                        T_UWORD msgqAreaSize);
T_TTOS_ReturnCode TTOS_GetMsgqCount(T_UWORD *msgqCount);
T_TTOS_ReturnCode TTOS_GetMsgqPendingCount(MSGQ_ID msgqID, T_UWORD *count);
T_TTOS_ReturnCode TTOS_GetMsgqInfo(MSGQ_ID msgqID, T_TTOS_MsgqInfo *msgqInfo);
T_TTOS_ReturnCode TTOS_GetMsgqName(MSGQ_ID msgqID, T_UBYTE *msgqName);
void ttosSetMessageNotify(T_TTOS_MsgqControlBlock *theMessageQueue, MSGQ_NOTIFY_HANDLER handler,
                          void *argument);
#endif /*TTOS_MSGQ*/

T_TTOS_ReturnCode TTOS_GetSystemTicks(T_UDWORD *ticks);

T_TTOS_ReturnCode TTOS_GetRunningTicks(T_UDWORD *ticks);
T_TTOS_ReturnCode TTOS_GetOneCpuRunningTicks(T_UDWORD *ticks);
T_TTOS_ReturnCode TTOS_GetIdleTaskExecutedTicks(T_UDWORD *ticks);
T_TTOS_ReturnCode TTOS_GetIdleTaskExecutedTicksWithCpuID(T_UWORD cpuID, T_UDWORD *ticks);
T_TTOS_ReturnCode TTOS_SetTaskExecutedTicks(TASK_ID taskID, T_UDWORD executedTicks);

T_TTOS_TaskControlBlock *ttosGetIdleTask(T_VOID);
T_TTOS_TaskControlBlock *ttosGetIdleTaskWithCpuID(T_UWORD cpuID);
T_VOID ttosGetTaskInfo(T_TTOS_TaskControlBlock *task, T_TTOS_TaskInfo *info);

#if CONFIG_SMP == 1
T_TTOS_TaskControlBlock *ttosGetUnReserveCpuLowestRTask(T_VOID);
#endif

#define TTOS_CopyName(dest, src) ttosCopyName(src, dest)

#define TTOS_CopyVersion(dest, src) ttosCopyVersion(src, dest)

T_UWORD TTOS_GetSysClkRate(T_VOID);

T_TTOS_ReturnCode TTOS_GetKernelInfo(T_TTOS_KernelBasicInfo *kernelInfo);
T_TTOS_ReturnCode TTOS_GetTaskExecutedTicks(TASK_ID taskID, T_UDWORD *executedTicks);

T_TTOS_ReturnCode TTOS_MapRamReadPage(T_ULONG logicalAddr, T_UDWORD phyAddr, T_ULONG size);

T_TTOS_ReturnCode TTOS_GetWaitLeastTicks(T_UWORD *waitTicks);
T_UDWORD TTOS_GetCurrentSystemCount(T_VOID);
T_UDWORD TTOS_GetCurrentSystemFreq(T_VOID);
T_UDWORD TTOS_GetCurrentSystemTime(T_UDWORD count, T_TTOS_TimeUnitType timeUnitType);
T_UDWORD TTOS_GetCurrentSystemSubTime(T_UDWORD count1, T_UDWORD count2,
                                      T_TTOS_TimeUnitType timeUnitType);
T_UWORD TTOS_GetNetCardVector(T_UWORD minor);

int occupy_tbl_get(T_ULONG *task_occupy, int maxAmount);
int wait_tbl_get(unsigned long *task_wait, int maxAmount);

T_TTOS_ReturnCode TTOS_SetTaskStdFd(TASK_ID taskID, T_UWORD std, T_UWORD fd);
T_TTOS_TaskControlBlock *ttosGetCurrentCpuRunningTask(T_VOID);

#if CONFIG_SMP == 1
T_TTOS_ReturnCode TTOS_SetTaskAffinity(TASK_ID taskID, cpu_set_t *affinity);
T_TTOS_ReturnCode TTOS_GetTaskAffinity(TASK_ID taskID, cpu_set_t *affinity);
int cpu_reserve(cpu_set_t cpus);
int cpu_unreserve(cpu_set_t cpus);
cpu_set_t cpuReservedSetGet(T_VOID);

int cpu_up(unsigned int cpu);
int cpu_down(unsigned int cpu);
#endif

T_UWORD cpuEnabledNumGet(T_VOID);
cpu_set_t cpuEnabledSetGet(T_VOID);

int period_thread_create(arch_exception_context_t *context);
int period_thread_wait_period(void);
unsigned long long get_sys_tick_ns(void);
T_TTOS_ReturnCode TTOS_SignalResumeTask(TASK_ID taskID);
T_TTOS_ReturnCode TTOS_SignalSuspendTask(TASK_ID taskID);
T_TTOS_ReturnCode task_delete_suspend(TASK_ID taskID);
T_TTOS_ReturnCode task_delete_resume(TASK_ID taskID);
T_VOID ttosPeriodPrioReadyQueueAdd(T_TTOS_TaskControlBlock *task);
T_VOID ttosPeriodPrioReadyQueueSetReady(T_VOID);
void arch_context_save_cpu_state(T_TTOS_TaskControlBlock *task, long msr);
void arch_context_restore_cpu_state(T_TTOS_TaskControlBlock *task);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
