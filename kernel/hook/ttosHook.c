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

/************************头 文 件******************************/
#include <assert.h>
#include <driver/cpudev.h>
#include <period_sched_group.h>
#include <string.h>
#include <system/kconfig.h>
#include <time.h>
#include <time/ktime.h>
#include <ttos.h>
#include <ttosBase.h>
#include <ttosProcess.h>

#undef KLOG_TAG
#define KLOG_TAG "Kernel"
#include <klog.h>

/************************宏 定 义******************************/
#define TASK_SWITCH_INFO KLOG_I
/************************类型定义******************************/
/************************外部声明******************************/
void ttos_set_task_timeout(TASK_ID task);

/************************前向声明******************************/
T_VOID TTOS_CreateTaskHook(TASK_ID taskID);
T_VOID TTOS_SwitchTaskHook(TASK_ID oldTask, TASK_ID newTask);

/************************模块变量******************************/
/************************全局变量******************************/
/************************实   现*******************************/
/*
 * @brief:
 *  在进行任务调度前被调用，用于用户根据实际情况做相应的处理。
 * @param[in]: mode:
 * 启动模式，此参数由应用开发者在调用TTOS_StartOS()进行DeltaTT初始化时指定
 * @return:
 *  无
 * @notes:
 *  本接口在禁止虚拟中断条件下调用。
 * @implements: DT.8.1
 */
T_VOID TTOS_StartHook(T_UWORD mode) {}

/*
 * @brief:
 *  在进行任务首次运行前被调用，用于用户根据实际情况做相应的处理。
 * @param[in]: taskID: 首次运行的任务ID
 * @return:
 *  无
 * @notes:
 *  本接口在禁止虚拟中断条件下调用。
 * @implements: DT.8.1
 */
T_VOID TTOS_TaskStartHook(TASK_ID taskID)
{
    struct T_TTOS_TaskControlBlock_Struct *task =
        (struct T_TTOS_TaskControlBlock_Struct *)ttosGetObjectById(taskID, TTOS_OBJECT_TASK);

    /* @REPLACE_BRACKET: 为进程 */
    if (task->ppcb != NULL)
    {
        pcb_t pcb = (pcb_t)task->ppcb;

        /* @KEEP_COMMENT: 设置其启动时间 */
        kernel_clock_gettime(CLOCK_MONOTONIC, &pcb->start_time);

        memset(&pcb->stime, 0, sizeof(struct timespec64));
        memset(&pcb->utime, 0, sizeof(struct timespec64));
    }
    else
    {
        /* @KEEP_COMMENT: 设置其启动时间 */
        kernel_clock_gettime(CLOCK_MONOTONIC, &task->start_time);
        memset(&task->stime, 0, sizeof(struct timespec64));
    }

    (void)ttosEnableTaskDispatchWithLock();
}
/*
 * @brief:
 *  任务被调度换入时被调用，用于用户根据实际情况做相应的处理。
 * @param[in]: taskID: 被调度换入的任务ID
 * @return:
 *  无
 * @notes:
 *  本接口在禁止任务调度条件下调用。
 * @implements: DT.8.4
 */
T_VOID TTOS_EnterTaskHook(TASK_ID taskID)
{
#if IS_ENABLED(CONFIG_CPU_USAGE_MONITOR)
    struct T_TTOS_TaskControlBlock_Struct *task = (struct T_TTOS_TaskControlBlock_Struct *)taskID;

    task->smpInfo.last_running_cpu = task->smpInfo.cpuIndex;

    if (ttosIsIdleTask(taskID))
    {
        cpu_enter_idle();
    }

    /* @REPLACE_BRACKET: 为进程 */
    if (task->ppcb != NULL)
    {
        pcb_t pcb = (pcb_t)task->ppcb;

        /* @KEEP_COMMENT: 获取当前时间并记录于pcb->stime_prev */
        kernel_clock_gettime(CLOCK_MONOTONIC, &pcb->stime_prev);
    }
    else if (!ttosIsIdleTask(taskID))
    {
        /* @KEEP_COMMENT: 获取当前时间并记录于pcb->stime_prev */
        kernel_clock_gettime(CLOCK_MONOTONIC, &task->stime_prev);
    }

    if (TTOS_SCHED_PERIOD == task->taskType)
    {
        if (!task->periodNode.periodStarted)
        {
            task->periodNode.periodStarted = true;
            task->periodNode.periodStartedTimestamp = TTOS_GetCurrentSystemCount();
        }

        task->periodNode.exeStartedTimestamp = TTOS_GetCurrentSystemCount();

        if (!period_sched_group_is_start(task->periodNode.periodTime))
        {
            period_sched_group_set_start(task->periodNode.periodTime);
        }
    }
#endif
}

/*
 * @brief:
 *  任务从用户态切换至核心态时被调用，用于用户根据实际情况做相应的处理。
 * @param[in]: taskID: 切换至核心态的任务ID
 * @return:
 *  无
 * @notes:
 *  本接口在禁止任务调度条件下调用。
 * @implements: DT.8.4
 */
T_VOID TTOS_TaskEnterKernelHook(TASK_ID taskID)
{
#if IS_ENABLED(CONFIG_CPU_USAGE_MONITOR)
    struct T_TTOS_TaskControlBlock_Struct *task = (struct T_TTOS_TaskControlBlock_Struct *)taskID;

    cpu_leave_user();

    /* @REPLACE_BRACKET: 为进程 */
    if (task->ppcb != NULL)
    {
        struct timespec64 tp;
        pcb_t pcb = (pcb_t)task->ppcb;

        /* @KEEP_COMMENT: 获取当前时间并记录于pcb->stime_prev */
        kernel_clock_gettime(CLOCK_MONOTONIC, &pcb->stime_prev);

        /* @KEEP_COMMENT: 计算上次在用户态运行的时间 */
        tp = clock_timespec_subtract64(&pcb->stime_prev, &pcb->utime_prev);

        /* @KEEP_COMMENT: 累加在用户态运行的时间 */
        pcb->utime = clock_timespec_add64(&tp, &pcb->utime);
    }
#endif
}

/*
 * @brief:
 *  任务从核心态切换至用户态时被调用，用于用户根据实际情况做相应的处理。
 * @param[in]: taskID: 切换至用户态的任务ID
 * @return:
 *  无
 * @notes:
 *  本接口在禁止任务调度条件下调用。
 * @implements: DT.8.4
 */
T_VOID TTOS_TaskEnterUserHook(TASK_ID taskID)
{
#if IS_ENABLED(CONFIG_CPU_USAGE_MONITOR)
    struct T_TTOS_TaskControlBlock_Struct *task = (struct T_TTOS_TaskControlBlock_Struct *)taskID;

    cpu_enter_user();

    /* @REPLACE_BRACKET: 为进程 */
    if (task->ppcb != NULL)
    {
        struct timespec64 tp;
        pcb_t pcb = (pcb_t)task->ppcb;

        /* @KEEP_COMMENT: 获取当前时间并记录于pcb->utime_prev */
        kernel_clock_gettime(CLOCK_MONOTONIC, &pcb->utime_prev);

        /* @KEEP_COMMENT: 计算上次在核心态运行的时间 */
        tp = clock_timespec_subtract64(&pcb->utime_prev, &pcb->stime_prev);

        /* @KEEP_COMMENT: 累加在核心态运行的时间 */
        pcb->stime = clock_timespec_add64(&tp, &pcb->stime);
    }
#endif
}

/*
 * @brief:
 *  任务被调度换出时被调用，用于用户根据实际情况做相应的处理。
 * @param[in]: taskID: 被调度换出的任务ID
 * @return:
 *  无
 * @notes:
 *  本接口在禁止任务调度条件下调用。
 *  本接口在禁止虚拟中断条件下调用。
 * @implements: DT.8.5
 */
T_VOID TTOS_LeaveTaskHook(TASK_ID taskID)
{
#if IS_ENABLED(CONFIG_CPU_USAGE_MONITOR)
    struct T_TTOS_TaskControlBlock_Struct *task = (struct T_TTOS_TaskControlBlock_Struct *)taskID;

#if defined(TTOS_SPINLOCK_WITH_RECURSION)
    if ((ttosKernelLockVar.nest_count > 2) && ttosKernelLockVar.cpu_id == cpuid_get())
    {
        assert(0 || "kernel spinlock error");
    }
#endif

    if (ttosIsIdleTask(taskID))
    {
        cpu_leave_idle();
    }

    /* task为 NULL 意味着任务已退出 */
    if (task == NULL)
    {
        return;
    }

    /* @REPLACE_BRACKET: 为进程 */
    if (task->ppcb != NULL)
    {
        pcb_t pcb = (pcb_t)task->ppcb;
        struct timespec64 tp;

        /* @KEEP_COMMENT: 获取当前时间并记录于tp */
        kernel_clock_gettime(CLOCK_MONOTONIC, &tp);

        tp = clock_timespec_subtract64(&tp, &pcb->stime_prev);

        pcb->stime = clock_timespec_add64(&tp, &pcb->stime);
    }
    else if (!ttosIsIdleTask(taskID))
    {
        struct timespec64 tp;

        /* @KEEP_COMMENT: 获取当前时间并记录于tp */
        kernel_clock_gettime(CLOCK_MONOTONIC, &tp);

        tp = clock_timespec_subtract64(&tp, &task->stime_prev);

        task->stime = clock_timespec_add64(&tp, &task->stime);
    }

    if (TTOS_SCHED_PERIOD == task->taskType)
    {
        task->periodNode.periodexecutecnt +=
            TTOS_GetCurrentSystemCount() - task->periodNode.exeStartedTimestamp;
    }
#endif
}

/*
 * @brief:
 *  周期任务截止期超时时被调用，用于用户根据实际情况做相应的处理。
 * @param[in]: taskID: 截止期超时的周期任务ID
 * @return:
 *  无
 * @notes:
 *  本接口在禁止任务调度条件下调用。
 *  本接口在禁止虚拟中断条件下调用。
 *  本接口中不能调用TTOS_ActiveTask()、TTOS_StopTask()、TTOS_ResetTask()、TTOS_ReplenishTask()来操作周期任务。
 * @implements: DT.8.6
 */
T_VOID TTOS_ExpireTaskHook(TASK_ID taskID)
{
    // KLOG_EMERG ("task %s is expired", taskID->objCore.objName);
    ttos_set_task_timeout(taskID);
}

/*
 * @brief:
 *  IDLE任务运行时被循环调用，用于用户根据实际情况做相应的处理。
 * @return:
 *  无
 * @implements: DT.8.3
 */
T_VOID TTOS_IdleHook(T_VOID)
{
    cpu_idle();
}

/*
 * @brief:
 *  TTOS分区应用调用TTOS_NotifyHM()通知TTOS健康监控处理错误时被调用，用于用户根据实际情况做相应的处理。
 * @param[in]: taskID: 产生错误的任务ID
 * @param[in]: action: 用户指定的健康监控处理动作
 * @return:
 *  无
 * @notes:
 *  <taskID>是调用TTOS_NotifyHM()时传入的，不一定是有效的ID。
 * @implements: DT.8.7
 */
T_VOID TTOS_HMHook(TASK_ID taskID, T_TTOS_HMAction action) {}

/*
 * @brief:
 *  TTOS分区应用任务执行完毕后被调用，用于用户根据实际情况做相应的处理。
 * @param[in]: taskID: 执行完毕的任务ID
 * @return:
 *  无
 * @notes:
 *  本接口在禁止任务调度条件下调用。
 * @implements: DT.8.8
 */
T_VOID TTOS_ExitTaskHook(TASK_ID taskID) {}

/*
 * @brief:
 *  当分区应用开发者调用TTOS_NotifyHM()通知健康监控使用关闭类健康监控处理
 *  动作处理错误时被调用，用于用户根据实际情况做相应的处理
 * @param[in]: action: 关闭类健康监控处理动作
 * @return:
 *  无
 * @notes:
 *  本接口在禁止虚拟中断条件下调用。
 * @implements: DT.8.2
 */
T_VOID TTOS_ShutdownHook(T_TTOS_HMAction action) {}

/**
 * @brief:
 *    TTOS初始化产生错误时被调用，用于用户根据实际情况做相应的处理。
 * @param[in]: funName: 产生错误的函数名
 * @param[in]: lineNum: 产生错误的行号
 * @param[in]: initErrorType: 初始化错误类型
 * @return:
 *    无
 * @notes:
 *    本接口在禁止虚拟中断条件下调用。
 * @implements: DT.8.9
 */
T_VOID TTOS_InitErrorHook(T_CONST T_CHAR *funName, T_WORD lineNum,
                          T_TTOS_Init_Eerros_List initErrorType)
{
#ifdef DEBUG_INFO
    KLOG_E("Init Error happend:the init Error Number is:%d .---in %s() line:%d.", initErrorType,
           funName, lineNum);
    KLOG_E("enter loop!");
    while (1)
    {
    }
#endif
}

/**
 * @brief:
 *    TTOS内部产生错误时被调用，用于用户根据实际情况做相应的处理。
 * @param[in]: funName: 产生错误的函数名
 * @param[in]: lineNum: 产生错误的行号
 * @param[in]: initErrorType: 初始化错误类型
 * @return:
 *    无
 * @notes:
 *    本接口在禁止虚拟中断条件下调用。
 * @implements: DT.8.9
 */
T_VOID TTOS_InternalErrorHook(T_CONST T_CHAR *funName, T_WORD lineNum,
                              T_TTOS_Internal_Eerros_List initErrorType)
{
#ifdef DEBUG_INFO
    KLOG_E("Internal Error happend:the internal Error Number is:%d .---in "
           "%s() line:%d.",
           initErrorType, funName, lineNum);
#endif
}

/*
 * @brief:
 *  任务创建时被调用，用于用户根据实际情况做相应的处理。
 * @param[in]: taskID: 将要创建的任务ID
 * @return:
 *  无
 * @notes:
 *  本接口在禁止任务调度条件下调用。
 * @implements: DT.8.8
 */
T_VOID TTOS_CreateTaskHook(TASK_ID taskID) {}

/*
 * @brief:
 *  任务切换时被调用，用于用户根据实际情况做相应的处理。
 * @param[in]: taskID: 将要切换到的任务ID
 * @return:
 *  无
 * @notes:
 *  本接口在禁止任务调度条件下调用。
 * @implements: DT.8.8
 */
T_VOID TTOS_SwitchTaskHook(TASK_ID oldTask, TASK_ID newTask) {}

const char *fault_str[] = {
    /* 禁止调度时产生的核心态CPU异常 */
    [TTOS_HM_DISABLE_DISPATCH_KERNEL_EXCEPTION] = "TTOS_HM_DISABLE_DISPATCH_KERNEL_EXCEPTION",

    /* 核心态下产生的二次CPU异常*/
    [TTOS_HM_SECOND_KERNEL_EXCEPTION] = "TTOS_HM_SECOND_KERNEL_EXCEPTION",

    /* IDLE任务产生的核心态CPU异常 */
    [TTOS_HM_IDLEVM_KERNEL_EXCEPTION] = "TTOS_HM_IDLEVM_KERNEL_EXCEPTION",

    /*一般核心态程序产生的CPU异常（除如上特殊情况下产生的核心态CPU异常） */
    [TTOS_HM_GENERAL_KERNEL_EXCEPTION] = "TTOS_HM_GENERAL_KERNEL_EXCEPTION",

    /*用户态程序产生的CPU异常 */
    [TTOS_HM_GENERAL_USER_EXCEPTION] = "TTOS_HM_GENERAL_USER_EXCEPTION",

    /* 系统运行过程中检查出的异常 */
    [TTOS_HM_RUNNING_CHECK_EXCEPTION] = "TTOS_HM_RUNNING_CHECK_EXCEPTION",

};

/*
 * @brief:
 *  系统严重故障时被调用，。
 * @param[in]: type: 故障类型
 * @param[in]: arg1: 参数1
 * @param[in]: arg2: 参数2
 * @param[in]: errorCode: 错误码
 * @return:
 *  无
 */
T_VOID osFaultHook(T_TTOS_HMType type, T_VOID *arg1, T_WORD arg2, T_WORD errorCode)
{
    KLOG_E("%s %s %d error code %d", fault_str[type], (char *)arg1, arg2, errorCode);
    while (1)
        ;
}

#define SCHED_INFO 0

#if SCHED_INFO

struct sched_info
{
    struct timespec64 sched_stime;
    struct timespec64 sched_max_time;
    struct timespec64 sched_time_prev;
    T_ULONG sched_count;
    struct timespec64 cand_time;
    struct timespec64 cand_max_time;
    struct timespec64 cand_time_prev;
    T_ULONG cand_count;
} g_sinfo[CONFIG_MAX_CPUS];

static int clock_timespec_cmp(const struct timespec64 *a, const struct timespec64 *b)
{
    if (a->tv_sec > b->tv_sec)
    {
        return 1;
    }
    else if (a->tv_sec < b->tv_sec)
    {
        return -1;
    }
    else
    {
        if (a->tv_nsec > b->tv_nsec)
        {
            return 1;
        }
        else if (a->tv_nsec < b->tv_nsec)
        {
            return -1;
        }
        else
        {
            return 0;
        }
    }
}
#endif

T_VOID TTOS_ScheduleEnterHook(T_VOID)
{
#if SCHED_INFO
    T_UWORD cpuID = cpuid_get();
    kernel_clock_gettime(CLOCK_MONOTONIC, &g_sinfo[cpuID].sched_time_prev);
#endif
}
T_VOID TTOS_ScheduleLeaveHook(T_VOID)
{
#if SCHED_INFO
    T_UWORD cpuID = cpuid_get();
    struct timespec64 tp;
    kernel_clock_gettime(CLOCK_MONOTONIC, &tp);
    tp = clock_timespec_subtract64(&tp, &g_sinfo[cpuID].sched_time_prev);
    if (clock_timespec_cmp(&tp, &g_sinfo[cpuID].sched_max_time) > 0)
    {
        g_sinfo[cpuID].sched_max_time = tp;
    }
    g_sinfo[cpuID].sched_stime = clock_timespec_add64(&tp, &g_sinfo[cpuID].sched_stime);
    g_sinfo[cpuID].sched_count++;
#endif
}
T_VOID TTOS_CandidateEnterHook(T_VOID)
{
#if SCHED_INFO
    T_UWORD cpuID = cpuid_get();
    kernel_clock_gettime(CLOCK_MONOTONIC, &g_sinfo[cpuID].cand_time_prev);
#endif
}
T_VOID TTOS_CandidateLeaveHook(T_VOID)
{
#if SCHED_INFO
    T_UWORD cpuID = cpuid_get();
    struct timespec64 tp;
    kernel_clock_gettime(CLOCK_MONOTONIC, &tp);
    tp = clock_timespec_subtract64(&tp, &g_sinfo[cpuID].cand_time_prev);
    if (clock_timespec_cmp(&tp, &g_sinfo[cpuID].cand_max_time) > 0)
    {
        g_sinfo[cpuID].cand_max_time = tp;
    }
    g_sinfo[cpuID].cand_time = clock_timespec_add64(&tp, &g_sinfo[cpuID].cand_time);
    g_sinfo[cpuID].cand_count++;
#endif
}

#if SCHED_INFO

#ifdef CONFIG_SHELL
#include <shell.h>

static int show_sched_info(void)
{
    int i;
    printk("\nCPUID    MAX_SCHED_TIME    MAX_CAND_TIME    AVE_SCHED_TIME    AVE_CAND_TIME    "
           "SCHED_COUNT    CAND_COUNT\n");
    for (i = 0; i < CONFIG_MAX_CPUS; i++)
    {
        if ((CPU_ISSET(i, TTOS_CPUSET_ENABLED())))
        {
            printk(
                "%5d    %14ldns  %13ldns  %14ldns  %13ldns  %11ld    %10ld\n", i,
                g_sinfo[i].sched_max_time.tv_sec * NSEC_PER_SEC + g_sinfo[i].sched_max_time.tv_nsec,
                g_sinfo[i].cand_max_time.tv_sec * NSEC_PER_SEC + g_sinfo[i].cand_max_time.tv_nsec,
                (g_sinfo[i].sched_stime.tv_sec * NSEC_PER_SEC + g_sinfo[i].sched_stime.tv_nsec) /
                    g_sinfo[i].sched_count,
                (g_sinfo[i].cand_time.tv_sec * NSEC_PER_SEC + g_sinfo[i].cand_time.tv_nsec) /
                    g_sinfo[i].cand_count,
                g_sinfo[i].sched_count, g_sinfo[i].cand_count);
        }
    }
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) |
                     SHELL_CMD_DISABLE_RETURN,
                 si, show_sched_info, show sched info);
#endif
#endif
// 网络包自检超时.
// 数据拥塞
