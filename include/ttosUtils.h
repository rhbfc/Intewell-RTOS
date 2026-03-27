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
 * @file： ttosUtils.h
 * @brief：
 *      <li>提供 Utils的相关的宏定义、类型定义和接口声明。</li>
 * @implements：
 */

#ifndef TTOSUTILS_H
#define TTOSUTILS_H

/************************头文件********************************/
#include <atomic.h>
#include <list.h>
#include <ttosTypes.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/************************宏定义********************************/
#define PACKED_BEGIN
#define PACKED __attribute__((__packed__))
#define PACKED_END

/* 对象名字长度，主要用于TTOS对象名 */
#define TTOS_OBJECT_NAME_LENGTH (32U)

#ifdef TTOS_32_PRIORTIY
/* TTOS只支持32级优先级 */
#define TTOS_PRIORITY_NUMBER (32U)

/* TTOS支持的最大优先级 */
#define TTOS_MAX_PRIORITY (31U)

/* 优先级任务队列分为4组，每组8个优先级，反向查找操作的优先级分界点为4 */
#define TTOS_QUEUE_DATA_NUMBER_OF_PRIORITY_HEADERS 4
#define TTOS_QUEUE_DATA_PRIORITIES_PER_HEADER 8
#define TTOS_QUEUE_DATA_REVERSE_SEARCH_LEVEL 0x4
#else
/* TTOS支持256级优先级 */
#define TTOS_PRIORITY_NUMBER (256U)

/* TTOS支持的最大优先级 */
#define TTOS_MAX_PRIORITY (255U)

/* 优先级任务队列分为4组，每组64个优先级，反向查找操作的优先级分界点为32 */
#define TTOS_QUEUE_DATA_NUMBER_OF_PRIORITY_HEADERS (4U)
#define TTOS_QUEUE_DATA_PRIORITIES_PER_HEADER (64U)
#define TTOS_QUEUE_DATA_REVERSE_SEARCH_LEVEL (0x20U)
#endif

/* TTOS支持的最小优先级 */
#define TTOS_MIN_PRIORITY (0U)

/* TTOS任务栈最小值 */

#include <limits.h>

#ifdef CONFIG_PROTECT_STACK

#define TTOS_MINIMUM_STACK_SIZE (4096 + PAGE_SIZE)

#else /* CONFIG_PROTECT_STACK */

#define TTOS_MINIMUM_STACK_SIZE 4096

#endif /* CONFIG_PROTECT_STACK */

/************************类型定义******************************/
typedef T_VOID (*T_TTOS_FREE_STACK_ROUTINE)(T_VOID *arg);

typedef struct TTOS_ObjectCore_Struct T_TTOS_ObjectCore;
typedef T_TTOS_ObjectCore *T_TTOS_ObjectCoreID;
typedef struct TTOS_Task_Queue_Control_Struct T_TTOS_Task_Queue_Control;
typedef T_TTOS_Task_Queue_Control wait_queue_head_t;
typedef struct T_TTOS_SemaControlBlock_Struct *MUTEX_ID;

/* 任务的资源节点 */
typedef struct
{
    /* 任务的资源节点  */
    struct list_node objectNode;

    /* 指向任务的资源节点的指针  */
    struct list_node *task;
} T_TTOS_ResourceTaskNode;

typedef struct
{
    /* 对象有没有被初始化 */
    T_ULONG magic;

    /* 对象类型 */
    T_ULONG type;
} T_TTOS_Handle;

typedef T_TTOS_Handle *T_TTOS_HandleId;

/* 对象的资源节点 */
typedef struct
{
    /* 任务的资源节点  */
    struct list_node objectResourceNode;

    /* 指向对象资源节点的指针  */
    struct list_node *objResourceObject;

} T_TTOS_ObjResourceNode;

/* 任务队列类型*/
typedef enum
{
    T_TTOS_QUEUE_DISCIPLINE_FIFO,    /* 先进先出队列类型 */
    T_TTOS_QUEUE_DISCIPLINE_PRIORITY /* 优先级队列类型 */

} T_TTOS_Task_Queue_Disciplines;

/* 任务状态 */
typedef enum
{
    /* 休眠态 */
    TTOS_TASK_DORMANT = 0x0001,

    /* 就绪态 */
    TTOS_TASK_READY = 0x00002,

    /* 运行态 */
    TTOS_TASK_RUNNING = 0x00004,

    /* 等待态 */
    TTOS_TASK_DELAYING = 0x00008,

    /* 挂起态 */
    TTOS_TASK_SUSPEND = 0x00010,

    /* 首次运行态 */
    TTOS_TASK_FIRSTRUN = 0x00020,

    /* 等待futex */
    TTOS_TASK_WAITING_FOR_FUTEX = 0x00040,

    /* 周期等待态 */
    TTOS_TASK_PWAITING = 0x00080,

    /* 完成等待态，即周期任务执行完成后还未到截止期，此时进入完成等待状态 */
    TTOS_TASK_CWAITING = 0x00100,

    /* 等待事件 */
    TTOS_TASK_WAITING_FOR_EVENT = 0x00200,

    /* 等待消息队列 */
    TTOS_TASK_WAITING_FOR_MSGQ = 0x00400,

    /* 等待信号量 */
    TTOS_TASK_WAITING_FOR_SEMAPHORE = 0x00800,

    /* 等待互斥锁 */
    TTOS_TASK_WAITING_FOR_MUTEX = 0x08000,

    /*内部转换状态*/
    TTOS_TASK_TRANSIENT = 0x01000,

    /*联合状态的任务在任务退出时等待其他任务执行pthread_join()来联合自身*/
    TTOS_TASK_WAITING_FOR_JOIN_AT_EXIT = 0x02000,

    /* 等待信号 */
    TTOS_TASK_WAITING_FOR_SIGNAL = 0x04000,

    /* 等待读写锁 */
    TTOS_TASK_WAITING_FOR_RWLOCK = 0x010000,

    /* 等待进程(waitpid) */
    TTOS_TASK_WAITING_FOR_PROCESS = 0x020000,

    /*
     *当前任务执行pthread_join()联合指定任务时，当前任务等待在指定联合任务的联合链表joinList上，
     *指定的联合任务退出时，才会将当前任务唤醒。
     */
    TTOS_TASK_WAITING_FOR_TASK_EXIT = 0x040000,

    /* 任务处于等待态时，可以被异步信号唤醒。*/
    TTOS_TASK_INTERRUPTIBLE_BY_SIGNAL = 0x080000,

    /* 等待time_event */
    TTOS_KTIMER_SLEEP = 0x100000,

    /* 等待completion */
    TTOS_TASK_WAITING_COMPLETION = 0x200000,

    /* 等待信号 */
    TTOS_TASK_WAITING_SIGNAL_PENDING = 0x400000,

    /* 被信号停止 */
    TTOS_TASK_STOPPED_BY_SIGNAL = 0x800000,

    /* 线程退出后等待被删除 */
    TTOS_TASK_WAITING_DELETE = 0x1000000,

    /* 任务已经退出，正在彻底清理，马上会被销毁 */
    TTOS_TASK_EXIT_DEAD = 0x2000000,

    /* 僵尸进程 */
    TTOS_TASK_EXIT_ZOMBIE = 0x4000000,

    /*等待在任务等待队列上*/
    TTOS_TASK_BLOCKING_ON_TASK_QUEUE =
        TTOS_TASK_WAITING_FOR_SEMAPHORE | TTOS_TASK_WAITING_FOR_MSGQ /*等待联合任务退出*/
        | TTOS_TASK_WAITING_FOR_TASK_EXIT | TTOS_TASK_WAITING_FOR_SIGNAL /* 等待信号 */
        | TTOS_TASK_WAITING_FOR_RWLOCK                                   /* 等待读写锁 */
        | TTOS_TASK_WAITING_FOR_PROCESS                                  /* 等待进程(waitpid) */
        | TTOS_TASK_WAITING_SIGNAL_PENDING | TTOS_TASK_WAITING_COMPLETION,

    /*任务等待指定资源*/
    TTOS_TASK_BLOCKING = TTOS_TASK_BLOCKING_ON_TASK_QUEUE |
                         TTOS_TASK_WAITING_FOR_EVENT /*等待联合任务退出*/
                         | TTOS_TASK_WAITING_FOR_TASK_EXIT | TTOS_TASK_WAITING_FOR_MUTEX |
                         TTOS_TASK_WAITING_FOR_FUTEX, /* 等待futex */

    /*任务等待指定时间或者等待资源*/
    TTOS_TASK_WAITING = TTOS_TASK_DELAYING | TTOS_TASK_BLOCKING |
                        TTOS_TASK_WAITING_FOR_JOIN_AT_EXIT | TTOS_TASK_INTERRUPTIBLE_BY_SIGNAL |
                        TTOS_KTIMER_SLEEP,
    /* 任务未处于运行状态标志 */
    TTOS_TASK_NONRUNNING_STATE = TTOS_TASK_DORMANT | TTOS_TASK_WAITING | TTOS_TASK_SUSPEND |
                                 TTOS_TASK_PWAITING | TTOS_TASK_CWAITING,
} T_TTOS_TaskState;

/* 任务队列控制结构 */
struct TTOS_Task_Queue_Control_Struct
{
    /* 使用联合结构便于统一任务队列链表结构 */
    union {
        struct list_head fifoQueue; /* 先进先出队列控制头 */
        struct list_head
            priorityQueue[TTOS_QUEUE_DATA_NUMBER_OF_PRIORITY_HEADERS]; /* 优先级任务队列的控制头
                                                                        */
    } queues;

    T_TTOS_Task_Queue_Disciplines discipline; /* 任务队列的类型，先进先出或者优先级方式 */
    T_TTOS_TaskState state;                   /* 处于任务队列时任务的状态 */
    T_TTOS_ObjectCore *objectCoreID;          /*等待队列对应的对象ID*/
};

struct TTOS_ObjectCore_Struct
{
    /* 链表节点 */
    struct list_node objectNode;

    T_TTOS_Handle handle;

    /* 对象名字 */
    T_UBYTE objName[TTOS_OBJECT_NAME_LENGTH + 1];

    /* 资源节点 */
    T_TTOS_ObjResourceNode objResourceNode;

    /* 引用计数 */
    atomic_t refcnt;

    /* 等待队列,用于等待对象释放 */
    wait_queue_head_t wq;
};

/* 所有API的返回值类型 */
typedef enum
{
    /* 操作成功 */
    TTOS_OK = 0,

    /* 操作失败 */
    TTOS_FAIL = 1,

    /* 无效ID */
    TTOS_INVALID_ID = 2,

    /* 无效名字 */
    TTOS_INVALID_NAME = 3,

    /* 无效地址 */
    TTOS_INVALID_ADDRESS = 4,

    /* 无效时间 */
    TTOS_INVALID_TIME = 5,

    /* 无效状态 */
    TTOS_INVALID_STATE = 6,

    /* 无效动作 */
    TTOS_INVALID_TYPE = 7,

    /* 没有请求到资源 */
    TTOS_UNSATISFIED = 8,

    /*无效的用户*/
    TTOS_INVALID_USER = 9,

    /*在中断中处理程序中执行*/
    TTOS_CALLED_FROM_ISR = 10,

    /*无效的大小*/
    TTOS_INVALID_SIZE = 11,

    /* 超时 */
    TTOS_TIMEOUT = 12,

    /* 内部错误*/
    TTOS_INTERNAL_ERROR = 13,

    /*无效的对齐 */
    TTOS_INVALID_ALIGNED = 14,

    /* 数值无效 */
    TTOS_INVALID_NUMBER = 15,

    /* 消息太多 */
    TTOS_TOO_MANY = 16,

    /* 对象已经被删除了 */
    TTOS_OBJECT_WAS_DELETED = 17,

    /* 无效的属性 */
    TTOS_INVALID_ATTRIBUTE = 18,

    /*无效的优先级 */
    TTOS_INVALID_PRIORITY = 19,

    /* 非资源拥有者 */
    TTOS_NOT_OWNER_OF_RESOURCE = 20,

    /* 资源正在被使用中 */
    TTOS_RESOURCE_IN_USE = 21,

    /* 对象版本不匹配 */
    TTOS_INVAILD_VERSION = 22,

    /*操作被屏蔽*/
    TTOS_MASKED = 23,

    /*无效的索引*/
    TTOS_INVALID_INDEX = 24,

    /*无效的系统调用*/
    TTOS_INVALID_SYSCALL = 25,

    /*不支持互斥信号量嵌套获取*/
    TTOS_MUTEX_NESTING_NOT_ALLOWED = 26,

    /*尝试获取互斥信号量的任务优先级高于当前互斥信号量天花板优先级*/
    TTOS_MUTEX_CEILING_VIOLATED = 27,

    /*互斥信号量嵌套层数超出最大允许值*/
    TTOS_MUTEX_NEST_OVERFLOW = 28,

    /* 对象已经被取消了 */
    TTOS_OBJECT_WAS_CANCELED = 29,

    /* 等待对象被信号中断 */
    TTOS_SIGNAL_INTR = 30,

} T_TTOS_ReturnCode;

/* 错误码类型 */
typedef enum
{
    /*设置定时器时间错误*/
    TTOS_SET_TIMER_TIME_ERROR = 1,

    /*获取调试设备中断线号错误*/
    TTOS_GET_DEBUG_DEVICE_INTNUM_ERROR = 2,

    /*设置的中断个数大于允许最大的中断个数*/
    TTOS_SET_ISR_NUM_ERROR = 3,

    /*设置的页表空间不够*/
    TTOS_SET_PAGE_RAM_SPACE_ERROR = 4,

    /*需要映射的空间已经映射*/
    TTOS_MAP_SPACE_ERROR = 4,

    /*任务以退出时，没有成功退出*/
    TTOS_TASK_ENTRY_RETURN_ERROR = 5,

    /*没有配置名字为VEC_BASE_ADRS_NAME的向量表空间*/
    TTOS_SET_VEC_SPACE_ERROR = 6,

    /*虚拟机上运行的TTOS试图访问MULTIBOOT_SCRATCH空间*/
    TTOS_VM_GET_MULTIBOOT_SCRATCH_SPACE_ERROR = 7,

#if CONFIG_SMP == 1
    /*内核锁获取错误*/
    TTOS_TAKE_SPIN_LOCK_ERROR = 8,

    /*内核锁释放错误*/
    TTOS_GIVE_SPIN_LOCK_ERROR = 9,
#endif

} T_TTOS_ErrorCode;

/* 健康监控中的异常类型 */
typedef enum
{
    /* 禁止调度时产生的核心态CPU异常 */
    TTOS_HM_DISABLE_DISPATCH_KERNEL_EXCEPTION = 1,

    /* 核心态下产生的二次CPU异常*/
    TTOS_HM_SECOND_KERNEL_EXCEPTION = 2,

    /* IDLE任务产生的核心态CPU异常 */
    TTOS_HM_IDLEVM_KERNEL_EXCEPTION = 3,

    /*一般核心态程序产生的CPU异常（除如上特殊情况下产生的核心态CPU异常） */
    TTOS_HM_GENERAL_KERNEL_EXCEPTION = 4,

    /*用户态程序产生的CPU异常 */
    TTOS_HM_GENERAL_USER_EXCEPTION = 5,

    /* 系统运行过程中检查出的异常 */
    TTOS_HM_RUNNING_CHECK_EXCEPTION = 6,

} T_TTOS_HMType;

/************************接口声明******************************/
T_VOID ttosCopyName(T_UBYTE *src, T_UBYTE *dest);
T_VOID ttosCopyVersion(T_UBYTE *src, T_UBYTE *dest);
T_BOOL ttosCompareName(T_UBYTE *name1, T_UBYTE *name2);
T_UWORD ttosCompareVerison(T_UBYTE *version1, T_UBYTE *version2);
T_VOID ttosSetFreeStackRoutine(T_TTOS_FREE_STACK_ROUTINE freeStackRoutine);
T_VOID taskDeleteSelfHandler(T_VOID *arg);
T_BOOL ttosObjectGet(T_TTOS_ObjectCoreID objectCore);
T_BOOL ttosObjectPut(T_TTOS_ObjectCoreID objectCore);
void ttosObjectWaitAndSetToFree(T_TTOS_ObjectCoreID objectCore, T_BOOL isInterruptible);
T_BOOL ttosSemaIsMutex(MUTEX_ID mutexID);
T_UBYTE *ttosGetRunningTaskName(T_VOID);

T_UDWORD ttosGetSystemTicks(T_VOID);
T_BOOL TTOS_IsAllRunningAndCandidateTaskIdleTask(T_VOID);
extern T_UWORD cpuid_get(T_VOID);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
