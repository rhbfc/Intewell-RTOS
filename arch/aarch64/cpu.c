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

/**
 * @file cpu.c
 * @brief AArch64 CPU 辅助接口。
 */

/************************头 文 件******************************/
#include <cpu.h>
#include <limits.h>
#include <stdbool.h>
#include <driver/cpudev.h>
/************************宏 定 义******************************/

/************************类型定义******************************/

/************************外部声明******************************/

/************************前向声明******************************/

/************************模块变量******************************/

/************************函数实现******************************/

/**
 * @brief
 *    设置当前 CPU ID
 * @retval 无
 */
void cpuid_set (void)
{
    struct cpudev *cpu = cpu_self_get();
    write_tpidr_el1(cpu->index);
}

/**
 * @brief
 *    获取当前 CPU ID
 * @param[in] 无
 * @retval 当前 CPU ID
 */
u32 cpuid_get (void)
{
    return read_tpidr_el1();
}

/*
 * @brief
 *    判断当前 CPU 是否为启动 CPU
 * @param[in] 无
 * @retval true 启动 CPU
 * @retval false 非启动 CPU
 */
bool is_bootcpu (void)
{
    struct cpudev *cpu = cpu_self_get();

    return cpu->boot_cpu;
}

unsigned long ttos_ffz(unsigned long x)
{
    return __builtin_ffsl(~x) - 1;
}
