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
#include <errno.h>
#include <ttos.h>
#include <ttosBase.h>
#include <ttosHal.h>

#define KLOG_LEVEL KLOG_WARN
#define KLOG_TAG   "Kernel"
#include <klog.h>

#if defined(CONFIG_SMP)
#include <ipi.h>

/* @MOD_HEAD> */

/************************宏 定 义******************************/
/*使用时间基计算的时间，单位为毫秒。*/
#define CPU_DOWN_WAIT_TIME_MS ULL (2000)

/************************类型定义******************************/
/************************外部声明******************************/
T_EXTERN cpu_set_t tickIsrIpiReschedulecpus;
T_EXTERN void      ttosDisableTaskDispatchWithLock (void);

/************************前向声明******************************/
/************************模块变量******************************/
/* 从CPU初始化栈栈顶 */
T_ULONG slaveCoreStackTop[CONFIG_MAX_CPUS] = { 0 };

/************************全局变量******************************/

/* @<MOD_VAR */

/* 已使能的CPU集合 */
static cpu_set_t ttosEnabledCpuSet;

/* 已预留的CPU集合 */
static cpu_set_t ttosReservedCpuSet;

/* 用户配置的cpu个数 */
T_UWORD ttosConfigCpuNum;

/* @MOD_VAR> */

/************************实    现******************************/

/* @MODULE> */

cpu_set_t *ttosGetEnabledCpuSet (void)
{
    return &ttosEnabledCpuSet;
}

cpu_set_t *ttosGetReservedCpuSet (void)
{
    return &ttosReservedCpuSet;
}

/*
 * @brief
 *       发送 IPI
 * @param[in]  cpus: 目的CPU集合。
 * @param[in]  ipiNum: IPI对应的中断号。
 * @param[in]  selfexcluded:发送IPI的目的CPU集合是否排除自己。
 * @return
 *   0:发送成功
 *   -1:发送失败
 * @notes:
 *     由调用接口保证此接口执行的原子性，即在执行过程中任务
 *不会 切走。
 */
T_WORD ipiEmit (cpu_set_t *cpus, T_UWORD ipiNum, T_BOOL selfexcluded)
{
    KLOG_E ("%s unimplement!!!", __func__);
    return (0);
}

/*
 * @brief
 *       发送重调度IPI
 * @param[in]  cpus: 目的CPU集合。
 * @param[in]  selfexcluded:发送IPI的目的CPU集合是否排除自己。
 * @return
 *   0:发送成功
 *   -1:发送失败
 */
INT32 ipiRescheduleEmit (cpu_set_t *cpus, T_BOOL selfexcluded)
{
    return ipi_reschedule (cpus, selfexcluded);
}

/*
 * @brief
 *    SMP初始化。
 * @param[in] : apCpuStart: 从核的起始CPU号。
 * @param[in] : cpuMask: 启动的CPU掩码。
 * @returns:
 *  无
 */
T_VOID smpInit (T_UWORD apCpuStart, T_UWORD cpuMask)
{
    KLOG_E ("%s unimplement!!!", __func__);
}

/*
 * @brief
 *    获取使能的CPU个数。
 * @returns:
 *       使能的CPU个数。
 */
T_UWORD cpuEnabledNumGet (void)
{
    return (CPU_COUNT (TTOS_CPUSET_ENABLED ()));
}

#endif
