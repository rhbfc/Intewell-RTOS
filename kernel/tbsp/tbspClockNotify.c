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

/* @<MOD_INFO */
/*
 * @file: DC_RT_6.pdl
 * @brief:
 *      <li>本模块为TTOS的板级支持包，为TTOS提供硬件访问支持。</li>
 *      <li>TBSP的核心是处理虚拟中断，TTOS初始化时接管tick中断。</li>
 * @implements: DT.6
 */
/* @MOD_INFO> */

/************************头 文 件******************************/

/* @<MOD_HEAD */
#include <commonTypes.h>
#include <int.h>
#include <ttosBase.h>
#include <time/ktime.h>
#include <time/ktimer.h>
#include <ttos_time.h>

#define KLOG_TAG "Kernel"
#include <klog.h>

/* @MOD_HEAD> */

/************************宏 定 义******************************/
#ifndef NSEC_PER_SEC
#define NSEC_PER_SEC 1000000000UL
#endif
#ifndef USEC_PER_SEC
#define USEC_PER_SEC 1000000UL
#endif
#ifndef MSEC_PER_SEC
#define MSEC_PER_SEC 1000UL
#endif

/************************类型定义******************************/
/************************外部声明******************************/
INT32 ipiRescheduleEmit (cpu_set_t *cpus, T_BOOL selfexcluded);
void period_sched_group_notify (T_UWORD ticks);
/************************前向声明******************************/
T_EXTERN void tbspClockNotify (void);
/************************模块变量******************************/

T_MODULE T_BOOL   ttosGetSytemFreqFlag = FALSE;
T_MODULE T_UDWORD tscFreq              = 0U;
/************************全局变量******************************/
/* @<MOD_VAR */
T_UWORD pwaitCurentTicks, pexpireCurrentTicks;

/* 系统tick计数值 */
T_MODULE T_UDWORD tickCount;

/* 分区tick计数值 */
T_MODULE T_UDWORD ttosTickCount;

/* 周期等待队列链上第一个周期任务的周期等待时间 */
T_UWORD pwaitTicks;

/*截止期等待队列链上第一个周期任务的截止期等待时间 */
T_UWORD pexpireTicks;

/*当前只有主核才处理虚拟tick中断。*/
cpu_set_t tickIsrIpiReschedulecpus;

/* @MOD_VAR> */

/************************实    现******************************/

/* @MODULE> */

/*
 * 当ttos运行在分区时，ticks数是一个相对Tick数，它是从SVMK中断派发当中传递
 * 过来的在SVMK调度分区、时间通知的过程中被计算出来。
 * 它用来表示，从当前分区主分区被切换出去到重新调度运行，DeltaTT总共流
 * 逝了多少Ticks数。
 */

/*
 * @brief:
 *    对任务进行时间的通知。
 * @param[in]: ticks: 从上次投递tick中断到本次投递tick中断期间当前分区主分区流逝
 * 的 tick数。
 * @return:
 *    无
 * @tracedREQ: RT.6
 * @implements: DT.6.8
 */
void tbspClockNotify (void)
{
    T_UWORD ticks = 1;
    /* @KEEP_COMMENT: 设置第一次进行时间通知的标志变量isFirstNotify为TRUE */
    T_MODULE T_BOOL isFirstNotify = TRUE;
    /* @KEEP_COMMENT: 设置周期等待通知到时剩余ticks变量pwaiteLeftTicks为0 */
    T_UWORD pwaiteLeftTicks = 0U;
    /* @KEEP_COMMENT:
     * 设置周期截止时间等待通知到时剩余ticks变量pexpireLeftTicks为0 */
    T_UWORD pexpireLeftTicks = 0U;
    /* @KEEP_COMMENT: 设置是否进行周期等待通知变量pwCurentTicksNotify为FALSE */
    T_BOOL pwCurentTicksNotify = FALSE;
    /* @KEEP_COMMENT:
     * 设置是否进行周期截止时间等待通知变量pexpireTicksNotify为FALSE */
    T_BOOL pexpireTicksNotify = FALSE;
    /* @KEEP_COMMENT:
     * 设置周期队列通知类型变量pQueueNotify为TTOS_PERIOD_NONE_QUEUE_NOTIFY */
    T_TTOS_PeriodQueueNotifyType pQueueNotify = TTOS_PERIOD_NONE_QUEUE_NOTIFY;

    /* @REPLACE_BRACKET: TRUE == isFirstNotify */
    if (TRUE == isFirstNotify)
    {
        /* @KEEP_COMMENT: 设置<ticks>为1 */
        ticks = 1U;
        /* @KEEP_CODE: */
        isFirstNotify = FALSE;
    }

    /* tick处理过程中是禁止中断的，所以此处不需要在禁止中断。*/
    TTOS_KERNEL_LOCK ();
    (void)ttosIncTickIntNestLevel ();
    tickCount += ticks;
    /* 记录真实投递给主分区的ticks，不加上流逝的ticks 。*/
    ttosTickCount++;
    /* @KEEP_COMMENT: 调用ttosTimerNotify(DT.4.10)实现定时器等待队列的时间通知
     */
    (void)ttosTimerNotify (ticks);
    /* @KEEP_COMMENT: 调用ttosTickNotify(DT.2.33)实现任务的tick时间通知 */
    (void)ttosTickNotify (ticks);

    (void)period_sched_group_notify(ticks);

    /* @REPLACE_BRACKET: 周期等待时间不为0 */
    if (pwaitCurentTicks != 0U)
    {
        /* @REPLACE_BRACKET: 周期等待时间大于<ticks> */
        if (pwaitCurentTicks > ticks)
        {
            /* @KEEP_COMMENT: 周期等待时间 = 周期等待时间 - <ticks> */
            pwaitCurentTicks -= ticks;
        }

        else
        {
            /* @KEEP_COMMENT: pwaiteLeftTicks = <ticks> - 周期等待通知时间 */
            pwaiteLeftTicks = ticks - pwaitCurentTicks;
            /* @KEEP_CODE: */
            pwCurentTicksNotify = TRUE;
            /* @KEEP_COMMENT: 设置周期等待时间为0 */
            pwaitCurentTicks = 0U;
            pwaitTicks       = 0U;
        }
    }

    /* @REPLACE_BRACKET: 截止期等待时间不为0 */
    if (pexpireCurrentTicks != 0U)
    {
        /* @REPLACE_BRACKET: 截止期等待时间大于<ticks> */
        if (pexpireCurrentTicks > ticks)
        {
            /* @KEEP_COMMENT: 截止期等待时间 = 截止期等待时间 - <ticks> */
            pexpireCurrentTicks -= ticks;
        }

        else
        {
            /* @KEEP_COMMENT: pexpireLeftTicks = <ticks> - 截止期等待通知时间 */
            pexpireLeftTicks = ticks - pexpireCurrentTicks;
            /* @KEEP_CODE: */
            pexpireTicksNotify = TRUE;
            /* @KEEP_COMMENT: 设置截止期等待时间为0 */
            pexpireCurrentTicks = 0U;
            pexpireTicks        = 0U;
        }
    }

    /* @REPLACE_BRACKET: TRUE == pwCurentTicksNotify */
    if (TRUE == pwCurentTicksNotify)
    {
        /*使用剩余时间来处理周期等待队列上的周期任务在绝对时间上应该处于的状态*/
        /* @KEEP_COMMENT: 调用ttosPeriodWaitQueueModify(DT.2.28)
         * 根据pwaiteLeftTicks来更新周期任务周期等待的时间 */
        (void)ttosPeriodWaitQueueModify (pwaiteLeftTicks);
    }

    /* @REPLACE_BRACKET: TRUE == pexpireTicksNotify */
    if (TRUE == pexpireTicksNotify)
    {
        /*使用剩余时间来处理截止期等待队列上的周期任务在绝对时间上应该处于的状态*/
        /* @KEEP_COMMENT: 调用ttosPeriodExpireQueueModify(DT.2.29)
         * 根据pexpireLeftTicks来更新周期任务截止期等待的时间 */
        (void)ttosPeriodExpireQueueModify (pexpireLeftTicks);
    }

    /* @REPLACE_BRACKET: (TRUE == pwCurentTicksNotify)||(TRUE ==
     * pexpireTicksNotify) */
    if ((TRUE == pwCurentTicksNotify) || (TRUE == pexpireTicksNotify))
    {
        /*
         * @KEEP_COMMENT: 调用ttosPeriodQueueMerge(DT.2.30)
         * 合并临时队列的周期任务到周期或者 截止时间等待队列上
         * ，并获取队列通知类型存放至pQueueNotify
         */
        pQueueNotify = ttosPeriodQueueMerge ();
    }

    /* @REPLACE_BRACKET: TRUE == pQueueNotify */
    switch (pQueueNotify)
    {
    case TTOS_PERIOD_WAIT_QUEUE_NOTIFY:
        /*
         * @KEEP_COMMENT: 调用ttosPeriodWaitNotify(DT.2.31)实现周期任务周期等待
         * 队列的时间通知
         */
        (void)ttosPeriodWaitNotify ();
        break;

    case TTOS_PERIOD_EXPIRE_QUEUE_NOTIFY:
        /*
         * @KEEP_COMMENT: 调用ttosPeriodExpireNotify(DT.2.27)实现周期任务截止时
         * 间等待队列的时间通知
         */
        (void)ttosPeriodExpireNotify ();
        break;

    case TTOS_PERIOD_ALL_QUEUE_NOTIFY:
        /*
         * @KEEP_COMMENT: 调用ttosPeriodWaitNotify(DT.2.31)实现周期任务周期等待
         * 队列的时间通知和调用ttosPeriodExpireNotify(DT.2.27)实现周期任务截止时
         * 间等待队列的时间通知
         */
        (void)ttosPeriodWaitNotify ();
        (void)ttosPeriodExpireNotify ();
        break;

    default:
        /* default */
        break;
    }

    ttosPeriodPrioReadyQueueSetReady();
    (void)ttosDecTickIntNestLevel ();
    TTOS_KERNEL_UNLOCK ();
}
/* @IGNORE_BEGIN: */
/**
 * @brief:
 *    获取系统tick数。
 * @param[out]: ticks: 存放系统tick的变量
 * @return:
 *    TTOS_OK:获取系统tick成功。
 * @tracedREQ:
 * @implements:
 */
T_TTOS_ReturnCode TTOS_GetSystemTicks (T_UDWORD *ticks)
{
    *ticks = tickCount;
    return TTOS_OK;
}

/*
 * @brief:
 *    获取系统tick数。
 * @return:
 *    系统tick数。
 */
T_UDWORD ttosGetSystemTicks (void)
{
    return (tickCount);
}

/**
 * @brief:
 *    获取分区运行tick数。
 * @param[out]: ticks: 存放分区运行tick的变量
 * @return:
 *    TTOS_OK:获取分区运行tick成功。
 * @tracedREQ:
 * @implements:
 */
T_TTOS_ReturnCode TTOS_GetRunningTicks (T_UDWORD *ticks)
{
    *ticks = ttosTickCount * cpuEnabledNumGet ();
    return TTOS_OK;
}

/**
 * @brief:
 *    获取分区在一个CPU上运行的tick数。
 * @param[out]: ticks: 存放分区运行tick的变量
 * @return:
 *    TTOS_OK:获取分区运行tick成功。
 * @tracedREQ:
 * @implements:
 */
T_TTOS_ReturnCode TTOS_GetOneCpuRunningTicks (T_UDWORD *ticks)
{
    *ticks = ttosTickCount;
    return TTOS_OK;
}

/**
 * @brief:
 *    获取每秒对应的tick数
 * @return:
 *    返回每秒tick数
 * @tracedREQ:
 * @implements:
 */
T_UWORD TTOS_GetSysClkRate (void)
{
    return NSEC_PER_SEC / CONFIG_SYS_TICK_NS;
}

/* @IGNORE_END: */
/*
 * @brief:
 *    设置周期等待队列的通知时间。
 * @param[in]: ticks: 周期等待队列的通知时间
 * @return:
 *    无
 * @tracedREQ: RT.6
 * @implements: DT.6.9
 */
void tbspSetPWaitTick (T_UWORD ticks)
{
    /* @KEEP_COMMENT: 设置周期等待队列通知时间为<ticks> */
    pwaitTicks = ticks;
    /* @KEEP_COMMENT: 设置周期等待队列等待时间为<ticks> */
    pwaitCurentTicks = ticks;
}

/*
 * @brief:
 *    设置截止期等待队列的通知时间。
 * @param[in]: ticks: 截止期等待队列的通知时间
 * @return:
 *    无
 * @tracedREQ: RT.6
 * @implements: DT.6.10
 */
void tbspSetPExpireTick (T_UWORD ticks)
{
    /* @KEEP_COMMENT: 设置截止期等待队列通知时间为<ticks> */
    pexpireTicks = ticks;
    /* @KEEP_COMMENT: 设置截止期等待队列等待时间为<ticks> */
    pexpireCurrentTicks = ticks;
}

/*
 * @brief:
 *    获取周期等待队列已等待的时间。
 * @return:
 *    周期等待队列已等待的时间
 * @tracedREQ: RT.6
 * @implements: DT.6.11
 */
T_UWORD tbspGetPWaitTick (void)
{
    /* @REPLACE_BRACKET: 周期等待队列通知时间减去周期等待队列等待时间 */
    return (pwaitTicks - pwaitCurentTicks);
}

/*
 * @brief:
 *    获取截止期等待队列已等待的时间。
 * @return:
 *    截止期等待队列已等待的时间
 * @tracedREQ: RT.6
 * @implements: DT.6.12
 */
T_UWORD tbspGetPExpireTick (void)
{
    /* @REPLACE_BRACKET: 截止期等待队列通知时间减去截止期等待队列等待时间 */
    return (pexpireTicks - pexpireCurrentTicks);
}

static void ttos_sys_tick_recover_expires (struct ktimer *ktimer)
{
    ktime_t next_expires = 0;
    ktime_t skip_expires = 0;
    int skip_count = 0;
    ktime_t now = ktime_now();

    next_expires = ktimer_get_expires(ktimer);
    if (next_expires <= now)
    {
        skip_count = ((now - next_expires) / CONFIG_SYS_TICK_NS + 1);
        skip_expires = skip_count * CONFIG_SYS_TICK_NS;
        ktimer_add_expires(ktimer, skip_expires);
    }

    if (skip_count)
    {
        KLOG_W("Tick Timer Skip %d Tick\n", skip_count);
    }
}

/*
 * @brief:
 *    系统定时器处理程序。
 * @return:
 *    无
 */

enum ktimer_restart ttos_sys_tick_handler (struct ktimer *ktimer)
{
    tbspClockNotify();

    ktimer_add_expires(ktimer, CONFIG_SYS_TICK_NS);

    /* 防御代码,保证极端情况下，系统依旧能够运行 */
    ttos_sys_tick_recover_expires (ktimer);

    return KTIMER_RESTART;
}

/*
 * @brief:
 *    系统TICK初始化。
 * @return:
 *    无
 */

void ttos_sys_tick_init (void)
{
    static struct ktimer sys_tick_timer;
    ktime_t now;

    /* 初始化定时器*/
    ktimer_init(&sys_tick_timer, ttos_sys_tick_handler, CLOCK_MONOTONIC, KTIMER_MODE_ABS);

    now = ktime_now();

    /* 启动定时器，100000ns后触发 */
    ktimer_start(&sys_tick_timer, now + 100000, KTIMER_MODE_ABS);
}

/*
 * @brief:
 *    初始化虚拟tick中断。
 * @return:
 *    无
 * @tracedREQ: RT.6
 * @implements: DT.6.13
 */
T_VOID tbspInitTimer (T_VOID)
{
    ttos_sys_tick_init();
}

/* @END_HERE: */
/*
 * @brief:
 *    获取当前系统中流逝的TSC COUNT数。
 * @return:
 *    当前系统中流逝的TSC COUNT数。
 */
T_UDWORD TTOS_GetCurrentSystemCount (void)
{
    T_UDWORD count = 0U;

    count = ttos_time_count_get ();
    return count;
}

/*
 * @brief:
 *    获取系统中TSC频率。
 * @return:
 *    系统中TSC频率。
 */
T_UDWORD TTOS_GetCurrentSystemFreq (void)
{
    T_UDWORD freq = 0U;

    if (FALSE == ttosGetSytemFreqFlag)
    {
        tscFreq              =  ttos_time_freq_get ();
        ttosGetSytemFreqFlag = TRUE;
    }

    freq = tscFreq;
    return freq;
}

/*
 * @brief:
 *    获取TSC COUNT对应流逝的时间。
 * @param[in]: count: TSC COUNT数
 * @param[in]: timeUnitType: 时间单位
 * @return:
 *    系统从启动流逝的时间。
 */
T_UDWORD TTOS_GetCurrentSystemTime (T_UDWORD            count,
                                    T_TTOS_TimeUnitType timeUnitType)
{
    T_UDWORD elapsed_time = 0ULL;

    if (FALSE == ttosGetSytemFreqFlag)
    {
        tscFreq              = TTOS_GetCurrentSystemFreq ();
        if(tscFreq > 0)
        {
            ttosGetSytemFreqFlag = TRUE;
        }
    }
    if (tscFreq > 0U)
    {
        switch (timeUnitType)
        {
        case TTOS_NS_UNIT:
            elapsed_time = (T_UDWORD)(count * (1.0 * NSEC_PER_SEC / tscFreq));
            break;

        case TTOS_US_UNIT:
            elapsed_time = (T_UDWORD)(count * (1.0 * USEC_PER_SEC / tscFreq));
            break;

        case TTOS_MS_UNIT:
            elapsed_time = (T_UDWORD)(count * (1.0 * MSEC_PER_SEC / tscFreq));
            break;

        default:
            elapsed_time = (T_UDWORD)(count / tscFreq);
            break;
        }
    }

    return elapsed_time;
}

/*
 * @brief:
 *    获取两个时间点的间隔时间
 * @param[in]: count1: 第一个时间点的TSC COUNT数
 * @param[in]: count2: 第二个时间点的TSC COUNT数
 * @param[in]: timeUnitType: 时间单位
 * @return:
 *    两个时间点的间隔时间
 */
T_UDWORD TTOS_GetCurrentSystemSubTime (T_UDWORD count1, T_UDWORD count2,
                                       T_TTOS_TimeUnitType timeUnitType)
{
    T_UDWORD interval_time = 0ULL;

    if (FALSE == ttosGetSytemFreqFlag)
    {
        tscFreq              = TTOS_GetCurrentSystemFreq ();
        ttosGetSytemFreqFlag = TRUE;
    }

    if (tscFreq != 0U)
    {
        switch (timeUnitType)
        {
        case TTOS_NS_UNIT:
            interval_time = (T_UDWORD)((count2 - count1)
                                       * (1.0 * NSEC_PER_SEC / tscFreq));
            break;

        case TTOS_US_UNIT:
            interval_time = (T_UDWORD)((count2 - count1)
                                       * (1.0 * USEC_PER_SEC / tscFreq));
            break;

        case TTOS_MS_UNIT:
            interval_time = (T_UDWORD)((count2 - count1)
                                       * (1.0 * MSEC_PER_SEC / tscFreq));
            break;

        default:
            interval_time = (T_UDWORD)((count2 - count1) * 1.0 / tscFreq);
            break;
        }
    }

    return interval_time;
}

unsigned long long get_sys_tick_ns (void)
{
    return CONFIG_SYS_TICK_NS;
}
