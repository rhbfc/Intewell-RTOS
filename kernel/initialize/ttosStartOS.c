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
#include <ttos.h>
#include <ttosBase.h>
#include <ttosHandle.h>
#include <ttosInterHal.h>
#include <ttosInterTask.inl>
#include <ttosMemProbe.h>
#include <ttosUtils.h>
#include <ttosUtils.inl>

#define KLOG_TAG "Kernel"
#include <klog.h>

/* @MOD_HEAD> */

/************************宏 定 义******************************/
#define TTOS_SMP_INFO KLOG_I

/************************类型定义******************************/
/************************外部声明******************************/

/* @<MOD_EXTERN */
/*周期临时等待队列链表头*/
T_EXTERN struct list_head periodWaitedQueueTmp;

/*
 *如果周期等待链上的周期任务直接加到截止期等待链上，
 *那么通知周期等待队列的时间在进行截止期等待队列通知时
 *也会通知此任务，会导致时间更快的效果，所以此处需要定
 *义临时截止期等待队列。
 */

/*周期截止时间临时等待队列链表头*/
T_EXTERN struct list_head expireWaitedQueueTmp;

/*是否栈填充*/
T_EXTERN T_BOOL ttosTaskStackFilledFlag;
T_EXTERN T_UBYTE pthreadPriorityToCore(int priority);

T_EXTERN T_VOID ttosDisableTaskDispatchWithLock(void);

T_EXTERN T_VOID fpscrInit(void);
T_EXTERN T_UWORD cpuNumGet(T_VOID);

/* @MOD_EXTERN> */

/************************前向声明******************************/
T_EXTERN T_VOID ttosAPEntry(T_VOID);
T_EXTERN void ttosIdleTaskEntry(void);
void ttos_create_init_task(void);
/************************模块变量******************************/
#if defined(CONFIG_SMP)
#if CONFIG_SMP == 1
/*记录从核是否已经开始初始化*/
T_MODULE BOOL ttosApInitialized[CONFIG_MAX_CPUS];
#endif
#endif

/************************全局变量******************************/

/* @<MOD_VAR */

/* ttos控制结构 */
T_TTOS_TTOSManager ttosManager;

#if defined(CONFIG_SMP)
/* IDLE任务控制结构 */
T_MODULE T_TTOS_TaskControlBlock ttosIdleTCB[CONFIG_MAX_CPUS];
#else  /*defined(CONFIG_SMP)*/
/* IDLE任务控制结构 */
T_MODULE T_TTOS_TaskControlBlock ttosIdleTCB;
#endif /*defined(CONFIG_SMP)*/

/* 处理任务删除自身的任务ID */
T_EXTERN TASK_ID ttosDeleteHandlerTaskID;

/*记录是否已经开始初始化*/
T_BOOL ttosInitialized = FALSE;

/* @MOD_VAR> */

/************************实    现******************************/

/* @MODULE> */

static void idle_task_init()
{
    T_UWORD currentCpu = 0U;
    T_TTOS_TaskControlBlock *idlePtr = NULL;
    currentCpu = cpuid_get();
    if (currentCpu > CONFIG_MAX_CPUS)
    {
        return;
    }
#if defined(CONFIG_SMP)
    idlePtr = &ttosIdleTCB[currentCpu];
#else  /*defined(CONFIG_SMP)*/
    idlePtr = &ttosIdleTCB;
#endif /*defined(CONFIG_SMP)*/
    (void)ttosInitTaskParam(idlePtr);
    /* @KEEP_COMMENT: 初始化IDLE任务 */
    idlePtr->taskControlId = (TASK_ID)idlePtr;
    (void)ttosCopyName((T_UBYTE *)TTOS_IDLETASK_NAME, idlePtr->objCore.objName);
    idlePtr->taskPriority = (T_UBYTE)(TTOS_PRIORITY_NUMBER - 1U);
    idlePtr->taskCurPriority = idlePtr->taskPriority;
    idlePtr->preemptCount = 0U;
    idlePtr->ppcb = NULL;
    idlePtr->arg = NULL;
    idlePtr->entry = NULL;
    idlePtr->taskType = TTOS_SCHED_NONPERIOD;
    idlePtr->tickSliceSize = TTOS_IDLETASK_SLICESIZE;
    idlePtr->tickSlice = TTOS_IDLETASK_SLICESIZE;
    /* @KEEP_COMMENT: 设置IDLE任务状态为就绪态 */
    idlePtr->state = (T_UWORD)TTOS_TASK_READY;
    idlePtr->stackStart = (T_UBYTE *)memalign(PAGE_SIZE, DEFAULT_TASK_STACK_SIZE);
    idlePtr->taskSchedPolicy = SCHED_RR;
    
    /* @REPLACE_BRACKET: NULL == idlePtr->stackStart */
    if (NULL == idlePtr->stackStart)
    {
        /*
         * @KEEP_COMMENT: 根据TTOS_INIT_MALLOC_ERROR调用初始化钩子函数
         * TTOS_InitErrorHook()
         */
        (void)TTOS_InitErrorHook(__FUNCTION__, __LINE__, TTOS_INIT_MALLOC_ERROR);
    }

    idlePtr->initialStackSize = DEFAULT_TASK_STACK_SIZE;
    idlePtr->stackBottom = idlePtr->stackStart;
    idlePtr->kernelStackTop = (T_UBYTE *)((T_ULONG)idlePtr->stackStart + idlePtr->initialStackSize -
                                          TTOS_SAFE_STACK_RESERVE_SIZE);
    idlePtr->switchContext.sp = (T_ULONG)idlePtr->kernelStackTop;
    CPU_SET(currentCpu, &idlePtr->smpInfo.affinity);
    idlePtr->smpInfo.affinityCpuIndex = currentCpu;
    /* @KEEP_COMMENT: 加入优先级队列 */
    ttosReadyQueuePut(idlePtr, FALSE);
    /* @KEEP_COMMENT: 加入优先级位图 */
    TBSP_SET_PRIORITYBITMAP(idlePtr->smpInfo.affinityCpuIndex, idlePtr->taskCurPriority);

    ttosSetRunningTaskWithCpuID(currentCpu, idlePtr);
    ttosSetHighestPriorityTask(currentCpu);
#if defined(CONFIG_SMP)
    ttosManager.idleTask[currentCpu] = idlePtr;
#else  /*defined(CONFIG_SMP)*/
    ttosManager.idleTask = idlePtr;
#endif /*defined(CONFIG_SMP)*/

    tid_obj_alloc(idlePtr);
    (void)ttosDisableTaskDispatchWithLock();
    ttosInitObjectCore(idlePtr, TTOS_OBJECT_TASK);
    (void)ttosEnableTaskDispatchWithLock();
}

/**
 * @brief:
 *    根据用户的配置，应对DeltaTT的管理结构、周期任务和非周期任务、IDLE任务、
 *    信号量、定时器和TBSP进行初始化，初始化完成后调用DeltaTT启动时机的钩子函数接口，并进行任务调度。
 * @param[in]: ttosConfigureTable: DeltaTT基本配置表
 * @param[in]: mode: 启动模式，提供给启动钩子函数TTOS_StartHook()使用
 * @return:
 *    无
 * @notes:
 *    本接口在禁止虚拟中断条件下调用。
 *    本接口在初始化完成后，进行任务调度前，使能虚拟中断。
 *    任务、信号量、定时器、板级支持包、事件记录只有在根据配置初始化成功完成后，才能被正常使用。
 *    第二次调用此接口，会直接返回，不进行任何操作。
 *    DeltaTT中每个对象的ID均使用无符号整型来表示，所以在空间允许的情况下，各个对象最多可以初始化4294967296个。
 *    任务名字、信号量名字和定时器名字最长32个字节(不包含串尾结束符)。
 * @implements: RTE_DINIT.1.1
 */
void TTOS_StartOS(T_TTOS_ConfigTable *ttosConfigureTable, T_UWORD mode)
{
    T_UWORD i;
    T_UWORD j;
    T_BOOL isPriorityCeiling;
    T_TTOS_SemaControlBlock *semaCB;
    T_TTOS_ConfigSema *semaPtr;
    T_TTOS_ReturnCode ret = TTOS_OK;
#ifdef TTOS_MSGQ
    /* -----------------初始化消息队列----------------------- */
    T_TTOS_MsgqControlBlock *msgqCB;
    T_TTOS_ConfigMsgq *msgqPtr;
#endif
    T_TTOS_TimerControlBlock *timerCB;
    T_TTOS_ConfigTimer *timerPtr;
    T_BOOL status;
    T_UWORD cpuNum = cpuNumGet();

    /*初始化只能执行一次*/
    /* @REPLACE_BRACKET: TTOS已经初始化 */
    if (TRUE == ttosInitialized)
    {
        return;
    }

    /* @KEEP_COMMENT: 设置ttosTaskStackFilledFlag是否对任务栈进行填充 */
    ttosTaskStackFilledFlag = ttosConfigureTable->taskStackFill;
    /* -------------初始化TTOS Manager结构 ----------------- */
    /* 初始化对象控制结构指针 */
    /* @KEEP_COMMENT: 初始化任务、信号量和定时器的控制结构指针 */
    ttosManager.taskCB = ttosConfigureTable->taskCB;
    ttosManager.semaCB = ttosConfigureTable->semaCB;
#ifdef TTOS_MSGQ
    ttosManager.msgqCB = ttosConfigureTable->msgqCB;
#endif
    ttosManager.timerCB = ttosConfigureTable->timerCB;
    /* 初始化对象数目 */
    /*
     *此处最大允许使用的任务对象数需要考虑系统默认创建的任务数，这样才能保证用户可
     *以创建的任务个数与配置的是一致的。
     */
    /* @KEEP_COMMENT: 初始化任务、信号量和定时器的个数 */
    ttosManager.objMaxNumber[TTOS_OBJECT_TASK] = ttosConfigureTable->configTaskNumber +
                                                 ttosConfigureTable->createTaskMaxNumber +
                                                 TTOS_DEFAULT_CREATE_TASK_NUM;
    ttosManager.objExistNumber[TTOS_OBJECT_TASK] = ttosConfigureTable->configTaskNumber;
    ttosManager.objMaxNumber[TTOS_OBJECT_SEMA] =
        ttosConfigureTable->configSemaNumber + ttosConfigureTable->createSemaMaxNumber;
    ttosManager.objExistNumber[TTOS_OBJECT_SEMA] = ttosConfigureTable->configSemaNumber;
    ttosManager.objMaxNumber[TTOS_OBJECT_TIMER] =
        ttosConfigureTable->configTimerNumber + ttosConfigureTable->createTimerMaxNumber;
    ttosManager.objExistNumber[TTOS_OBJECT_TIMER] = ttosConfigureTable->configTimerNumber;
#ifdef TTOS_MSGQ
    ttosManager.objMaxNumber[TTOS_OBJECT_MSGQ] =
        ttosConfigureTable->configMsgqNumber + ttosConfigureTable->createMsgqMaxNumber;
    ttosManager.objExistNumber[TTOS_OBJECT_MSGQ] = ttosConfigureTable->configMsgqNumber;
#endif

    /* @REPLACE_BRACKET: TTOS_OK != ret */
    if (TTOS_OK != ret)
    {
        /* @KEEP_COMMENT: 禁止虚拟中断条件下调用 */
        (void)TTOS_InitErrorHook(__FUNCTION__, __LINE__, TTOS_INIT_WORKSPACE_ERROR);
    }

    /* @KEEP_COMMENT: 初始化任务tick等待队列ttosManager.taskWaitedQueue */
    INIT_LIST_HEAD(&ttosManager.taskWaitedQueue);
    /* @KEEP_COMMENT: 初始化定时器等待队列ttosManager.timerWaitedQueue */
    INIT_LIST_HEAD(&ttosManager.timerWaitedQueue);
    /* @KEEP_COMMENT: 初始化周期任务周期等待队列ttosManager.periodWaitedQueue */
    INIT_LIST_HEAD(&ttosManager.periodWaitedQueue);
    /* @KEEP_COMMENT:
     * 初始化周期任务截止时间等待队列ttosManager.expireWaitedQueue */
    INIT_LIST_HEAD(&ttosManager.expireWaitedQueue);
    /* @KEEP_COMMENT: 初始化周期任务周期临时等待队列periodWaitedQueueTmp */
    INIT_LIST_HEAD(&periodWaitedQueueTmp);
    /* @KEEP_COMMENT: 初始化周期任务截止时间临时等待队列expireWaitedQueueTmp */
    INIT_LIST_HEAD(&expireWaitedQueueTmp);
    /* @KEEP_COMMENT:初始化任务删除自身的队列ttosManager.deleteSelfTaskIdQueue
     */
    INIT_LIST_HEAD(&(ttosManager.deleteSelfTaskIdQueue));

    /* 初始化任务优先级就绪队列 */
    /* @REPLACE_BRACKET: 变量i = 0; i < TTOS_PRIORITY_NUMBER; i++ */
    for (i = 0U; i < cpuNum; i++)
    {
#if defined(CONFIG_SMP)
        ttosManager.priorityQueue[i] = (struct list_head *)memalign(
            DEFAULT_TASK_STACK_ALIGN_SIZE, sizeof(struct list_head) * TTOS_PRIORITY_NUMBER);

        /* @REPLACE_BRACKET: NULL == ttosManager.priorityQueue[i] */
        if (NULL == ttosManager.priorityQueue[i])
        {
            /* @KEEP_COMMENT: 禁止虚拟中断条件下调用 */
            (void)TTOS_InitErrorHook(__FUNCTION__, __LINE__, TTOS_INIT_MALLOC_ERROR);
        }

#endif

        /* @REPLACE_BRACKET: j = U(0); j < TTOS_PRIORITY_NUMBER; j++ */
        for (j = 0U; j < TTOS_PRIORITY_NUMBER; j++)
        {
#if defined(CONFIG_SMP)
            /* @KEEP_COMMENT:
             * 初始化优先级为j级的就绪队列ttosManager.priorityQueue[j] */
            INIT_LIST_HEAD(&(ttosManager.priorityQueue[i][j]));
#else  /*defined(CONFIG_SMP)*/
            /* @KEEP_COMMENT:
             * 初始化优先级为j级的就绪队列ttosManager.priorityQueue[i][j] */
            INIT_LIST_HEAD(&(ttosManager.priorityQueue[j]));
#endif /*defined(CONFIG_SMP)*/
        }
    }
#if defined(CONFIG_SMP)

    /* @REPLACE_BRACKET: i = U(0); i < TTOS_PRIORITY_NUMBER; i++ */
    for (i = 0U; i < TTOS_PRIORITY_NUMBER; i++)
    {
        /* @KEEP_COMMENT: 初始化优先级为i级的全局就绪队列 */
        INIT_LIST_HEAD(&(ttosManager.globalPriorityQueue[i]));
    }

#endif

    /* @REPLACE_BRACKET: i = U(0); i <= TTOS_OBJECT_CHAIN_LAST; i++ */
    for (i = 0U; i <= (T_UWORD)TTOS_OBJECT_CHAIN_LAST; i++)
    {
        /* @KEEP_COMMENT: 初始化优先级为i级的全局空闲的对象资源 */
        INIT_LIST_HEAD(&(ttosManager.inactiveResource[i]));
        INIT_LIST_HEAD(&(ttosManager.inUsedResource[i]));
        ttosManager.inUsedResourceNum[i] = 0U;
    }

    /* @KEEP_COMMENT: 关调度，初始化过程不允许进行调度 */
    (void)ttosDisableTaskDispatchWithLock();
    /* -------------初始化任务管理 ---------------------- */
    /* @KEEP_COMMENT: 初始化IDLE任务。*/
    idle_task_init();

    /* @KEEP_COMMENT: 初始化init任务。*/
    ttos_create_init_task();

#ifdef TTOS_MSGQ
    msgqCB = ttosManager.msgqCB;
    msgqPtr = ttosConfigureTable->msgqConfig;
    /* @REPLACE_BRACKET: msgqPtr为空地址 */
    if (msgqPtr == NULL)
    {
        (void)TTOS_InitErrorHook(__FUNCTION__, __LINE__, TTOS_INIT_OTHER_ERROR);
    }

    /* @KEEP_COMMENT: 根据配置表的属性初始化第i个消息队列 */
    for (i = U(0); i < ttosConfigureTable->configMsgqNumber; i++)
    {
        /* @KEEP_COMMENT: 判断名字字符串是否符合命名规范，返回值赋值给status */
        status = ttosVerifyName(msgqPtr->cfgMsgqName, TTOS_OBJECT_NAME_LENGTH);

        /* @REPLACE_BRACKET: status不正确 */
        if (FALSE == status)
        {
            /*
             * @KEEP_COMMENT: 根据TTOS_CONFIG_NAME_ERROR调用初始化钩子函数
             * TTOS_InitErrorHook()
             */
            TTOS_InitErrorHook(__FUNCTION__, __LINE__, TTOS_CONFIG_NAME_ERROR);
        }

        /* @KEEP_COMMENT:消息队列配置的消息个数是否合法 */
        /* @REPLACE_BRACKET: TTOS_INIT_COUNT == msgqPtr->maxMsgqNumber */
        if (TTOS_INIT_COUNT == msgqPtr->maxMsgqNumber)
        {
            /*
             * @KEEP_COMMENT: 根据TTOS_CONFIG_COUNT_ERROR调用初始化钩子函数
             * TTOS_InitErrorHook()
             */
            TTOS_InitErrorHook(__FUNCTION__, __LINE__, TTOS_CONFIG_COUNT_ERROR);
        }

        /* @KEEP_COMMENT: 消息队列配置的消息大小是否合法 */
        /* @REPLACE_BRACKET: TTOS_INIT_SIZE == msgqPtr->maxMsgSize */
        if (TTOS_INIT_SIZE == msgqPtr->maxMsgSize)
        {
            /*
             * @KEEP_COMMENT: 根据TTOS_CONFIG_SIZE_ERROR调用初始化钩子函数
             * TTOS_InitErrorHook()
             */
            TTOS_InitErrorHook(__FUNCTION__, __LINE__, TTOS_CONFIG_SIZE_ERROR);
        }
        /* @KEEP_COMMENT: 初始化消息队列，返回值赋值给ret */
        ret = ttosInitMsgq(msgqCB, msgqPtr, NULL);

        /* @REPLACE_BRACKET: TTOS_OK != ret */
        if (TTOS_OK != ret)
        {
            /*
             * @KEEP_COMMENT: 根据TTOS_CONFIG_INIT_MSGQ_ERROR调用初始化钩子函数
             * TTOS_InitErrorHook()
             */
            TTOS_InitErrorHook(__FUNCTION__, __LINE__, TTOS_CONFIG_INIT_MSGQ_ERROR);
        }

        msgqCB++;
        msgqPtr++;
    }

#endif
    /* -----------------初始化信号量----------------------- */
    /* @KEEP_COMMENT: 获取信号量的控制结构和配置结构指针 */
    semaCB = ttosManager.semaCB;
    semaPtr = ttosConfigureTable->semaConfig;
    /* @REPLACE_BRACKET: semaPtr为空地址 */
    if (semaPtr == NULL)
    {
        TTOS_InitErrorHook(__FUNCTION__, __LINE__, TTOS_INIT_OTHER_ERROR);
    }

    /* @KEEP_COMMENT: 根据配置表的属性初始化第i个信号量 */
    /* @REPLACE_BRACKET: i = 0; i <
     * DeltaTT配置的任务个数ttosConfigTable.configSemaNumber; i++ */
    for (i = 0U; i < ttosConfigureTable->configSemaNumber; i++)
    {
        /* @KEEP_COMMENT: 判断名字字符串是否符合命名规范*/
        status = ttosVerifyName(semaPtr->cfgSemaName, TTOS_OBJECT_NAME_LENGTH);

        /* @REPLACE_BRACKET: FALSE == status */
        if (FALSE == status)
        {
            /*
             * @KEEP_COMMENT: 根据TTOS_CONFIG_NAME_ERROR调用初始化钩子函数
             * TTOS_InitErrorHook()
             */
            (void)TTOS_InitErrorHook(__FUNCTION__, __LINE__, TTOS_CONFIG_NAME_ERROR);
        }

        isPriorityCeiling = FALSE;

        /* @KEEP_COMMENT: 根据信号量类型进行检查 */
        /* @REPLACE_BRACKET: semaPtr->attributeSet & TTOS_SEMAPHORE_MASK */
        switch (semaPtr->attributeSet & TTOS_SEMAPHORE_MASK)
        {
        /* @KEEP_COMMENT: 计数信号量直接跳出 */
        case TTOS_COUNTING_SEMAPHORE:
            break;

        /* @KEEP_COMMENT: 互斥信号需要判断初始值 */
        case TTOS_MUTEX_SEMAPHORE:
            /* @REPLACE_BRACKET: 信号量初始化值不在允许范围内 */
            if (semaPtr->initValue > 1U)
            {
                /*
                 * @KEEP_COMMENT: 根据TTOS_CONFIG_COUNT_ERROR调用初始化钩子函数
                 * TTOS_InitErrorHook()
                 */
                (void)TTOS_InitErrorHook(__FUNCTION__, __LINE__, TTOS_CONFIG_COUNT_ERROR);
            }

            isPriorityCeiling = TRUE;
            break;

        /* @KEEP_COMMENT: 无效的信号量类型 */
        default:
            /*
             * @KEEP_COMMENT: 根据TTOS_CONFIG_ATTRIBUTE_ERROR调用初始化钩子函数
             * TTOS_InitErrorHook()
             */
            (void)TTOS_InitErrorHook(__FUNCTION__, __LINE__, TTOS_CONFIG_ATTRIBUTE_ERROR);
            break;
        }

        (void)ttosInitSema(semaCB, semaPtr, isPriorityCeiling);
        semaPtr++;
        semaCB++;
    }

    /* -----------------初始化定时器--------------------- */
    /* @KEEP_COMMENT: 获取定时器的控制结构和配置结构指针 */
    timerCB = ttosManager.timerCB;
    timerPtr = ttosConfigureTable->timerConfig;
    /* @REPLACE_BRACKET: timerPtr为空地址 */
    if (timerPtr == NULL)
    {
        TTOS_InitErrorHook(__FUNCTION__, __LINE__, TTOS_INIT_OTHER_ERROR);
    }

    /* @REPLACE_BRACKET: i = 0; i <
     * DeltaTT配置的任务个数ttosConfigTable.configTimerNumber; i++ */
    for (i = 0U; i < ttosConfigureTable->configTimerNumber; i++)
    {
        /* @KEEP_COMMENT: 根据配置表的属性初始化第i个定时器 */
        /* @KEEP_COMMENT: 判断名字字符串是否符合命名规范*/
        status = ttosVerifyName(timerPtr->cfgTimerName, TTOS_OBJECT_NAME_LENGTH);

        /* @REPLACE_BRACKET: FALSE == status */
        if (FALSE == status)
        {
            /*
             * @KEEP_COMMENT: 根据TTOS_CONFIG_NAME_ERROR调用初始化钩子函数
             * TTOS_InitErrorHook()
             */
            (void)TTOS_InitErrorHook(__FUNCTION__, __LINE__, TTOS_CONFIG_NAME_ERROR);
        }

        /* @REPLACE_BRACKET: 定时器i开启自动激活 */
        if (TRUE == timerPtr->autoStarted)
        {
            /*
             *change by sunsc on
             *2017-4-13:为了保持与其他定时器相关的接口一致性，
             *定时器不允许配置时间间隔为0。
             */
            /* @REPLACE_BRACKET: 定时器i的处理函数为空或者定时器时间间隔tick为0
             */
            if ((NULL == timerPtr->handler) || (0U == timerPtr->ticks))
            {
                /*
                 * @KEEP_COMMENT: 根据TTOS_CONFIG_TIMER_ERROR调用初始化钩子函数
                 * TTOS_InitErrorHook()
                 */
                (void)TTOS_InitErrorHook(__FUNCTION__, __LINE__, TTOS_CONFIG_TIMER_ERROR);
            }
        }

        (void)ttosInitTimer(timerCB, timerPtr);
        timerPtr++;
        timerCB++;
    }

    /* -------------------初始化TBSP-------------------- */
    /* @KEEP_COMMENT: 调用tbspInit(DT.6.7)初始化TBSP */
    tbspInit();

    /* --------------------调用启动勾子函数-------------- */
    /* @KEEP_COMMENT: 调用TTOS_StartHook()启动勾子函数 */
    (void)TTOS_StartHook(mode);
    (void)ttosSetPWaitAndExpireTick();
    /* @KEEP_COMMENT: 设置ttosInitialized为已经初始化的 */
    ttosInitialized = TRUE;
    /* ---------------------使能中断----------------- */
    /* @KEEP_COMMENT: 开启虚拟中断 */
    TBSP_GLOBALINT_OPEN();

    /* ---------------------进行重新调度----------------- */
    /* @KEEP_COMMENT: 调用ttosEnableTaskDispatch()进行重新调度 */
    (void)ttosEnableTaskDispatchWithLock();
#ifdef _HARD_FLOAT_
    /* @KEEP_COMMENT: 调用fpscrInit()初始化FPSCR寄存器 */
    fpscrInit();
#endif
#if defined(CONFIG_SMP)
    idleTaskEntryLoad(ttosManager.idleTask[cpuid_get()]->kernelStackTop);
#else  /*defined(CONFIG_SMP)*/
    (void)idleTaskEntryLoad(idlePtr->kernelStackTop);
#endif /*defined(CONFIG_SMP)*/
}

#if defined(CONFIG_SMP)
/*
 * @brief:
 *    AP启动后，入口函数，完成相关初始化操作。
 * @return:
 *    无
 * @implements: RTE_DINIT.1.2
 */
T_VOID ttosAPEntry(T_VOID)
{
    T_UWORD currentCpu = 0U;
    currentCpu = cpuid_get();

    /* @KEEP_COMMENT: 初始化cpu */
    ttosApInitialized[currentCpu] = TRUE;

    /* @KEEP_COMMENT: 初始化IDLE任务。*/
    idle_task_init();

    /* @KEEP_COMMENT: 将当前cpu号加入到cpu使能集合中 */
    (T_VOID) CPU_SET(currentCpu, TTOS_CPUSET_ENABLED());

    /* @KEEP_COMMENT: 进行任务调度 */
    ttosSchedule();

    /* @KEEP_COMMENT: 使能CPU中断 */
    TBSP_GLOBALINT_OPEN();

    /* @KEEP_COMMENT: 加载进入到IDLE任务 */
    idleTaskEntryLoad(ttosManager.idleTask[currentCpu]->kernelStackTop);
}
#endif /*defined(CONFIG_SMP)*/

/*
 * @brief:
 *    IDLE分区的运行实体。
 * @return:
 *    无
 * @implements: RTE_DINIT.1.3
 */
void ttosIdleTaskEntry(void)
{
    /* ---------------------IDLE 任务的主循环-------------- */
    /* @REPLACE_BRACKET: while (1) */
    while (1)
    {
        /* @KEEP_COMMENT: 调用TTOS_IdleHook()进行IDLE任务的主循环 */
        (void)TTOS_IdleHook();
    }
}
