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

#ifndef _CPU_ARM_H
#define _CPU_ARM_H

/************************头文件********************************/
#include <driver/cpudev.h>
#include <driver/platform.h>
#include <system/macros.h>
#include <system/types.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/************************类型定义******************************/
struct arm_cpu_priv
{
    uint64_t affinity;
};

/************************接口声明******************************/
/* ttos-n\drivers\cpu\method-ops\ */
void cpu_method_ops_match(struct device *dev);
int psci_method_init(struct cpudev *cpu);
int spin_method_init(struct cpudev *cpu);
int fmsh_method_init(struct cpudev *cpu);

/* ttos-n\drivers\cpu\ */
int cpu_armv7_affinity_set(struct device *dev);
int cpu_armv8_affinity_set(struct device *dev);
bool cpu_armv7_is_self(struct cpudev *cpu);
bool cpu_armv8_is_self(struct cpudev *cpu);
uint64_t cpu_arm_hw_id_get(struct cpudev *cpu);
void cpu_arm_idle(struct cpudev *cpu);
int cpu_arm_probe(struct platform_device *pdev);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _CPU_ARM_H */
