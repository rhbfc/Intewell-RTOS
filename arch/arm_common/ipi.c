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
#include <system/bitops.h>
#include <errno.h>
#include <ipi.h>
#include <ttos.h>
#include <ttosBase.h>
#include <ttos_pic.h>
#include <system/types.h>

/************************宏 定 义******************************/

/************************类型定义******************************/

/************************外部声明******************************/

/************************前向声明******************************/

/************************模块变量******************************/

/************************全局变量******************************/

/************************实   现*******************************/

/**
 * @brief
 *	  发送IPI
 * @param[in] cpus 目的CPU集合。为0时，表示系统中已经使能的CPU
 * @param[in] ipi ipi中断号
 * @param[in] selfexcluded 发送IPI的目的CPU集合是否排除自己
 * @retval 0 成功
 * @retval EIO 失败
 */
static s32 ipi_send (cpu_set_t *cpus, u32 ipi, bool selfexcluded)
{
    cpu_set_t target_cpus;
    CPU_ZERO (&target_cpus);

    /* 获取目的cpu集合 */
    if (CPU_COUNT (cpus) == 0)
    {
#if CONFIG_SMP == 1
        CPU_OR (&target_cpus, &target_cpus, TTOS_CPUSET_ENABLED ());
#else
        CPU_ZERO (&target_cpus);
        CPU_SET (0, &target_cpus);
#endif
    }
    else
    {
        CPU_OR (&target_cpus, &target_cpus, cpus);
    }

    /* 是否排除自己 */
    if (TRUE == selfexcluded)
    {
        CPU_CLR (cpuid_get (), &target_cpus);
    }

    if (CPU_COUNT (&target_cpus) == 0)
    {
        return (-EIO);
    }

    /* 获取有效位的cpu索引号 */
    for (int cpu = 0; cpu < CONFIG_MAX_CPUS; cpu++)
    {
        if (CPU_ISSET (cpu, &target_cpus))
        {
            if ((GENERAL_IPI_SCHED == ipi)
                && (FALSE == ttosIsNeedRescheduleWithCpuId (cpu)))
            {
                continue;
            }

            /* 发送ipi */
            ttos_pic_ipi_send (ipi, cpu, 0);
        }
    }

    return (0);
}

/**
 * @brief
 *	  发送重调度IPI
 * @param[in] cpus 目的CPU集合。为0时，表示系统中已经使能的CPU
 * @param[in] selfexcluded 发送IPI的目的CPU集合是否排除自己
 * @retval 0 成功
 * @retval EIO 失败
 */
s32 ipi_reschedule (cpu_set_t *cpus, bool selfexcluded)
{
    return ipi_send (cpus, GENERAL_IPI_SCHED, selfexcluded);
}

/**
 * @brief
 *	  重调度IPI处理函数，对于目前的调度策略，为空
 * @param[in] irq 中断号
 * @param[in] 私有数据
 * @retval 无
 */
void ipi_reschedule_handler (u32 irq, void *param) {}
