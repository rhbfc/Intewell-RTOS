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

#include <arch_helpers.h>
#include <driver/cpu/cpu_arm.h>
#include <driver/cpudev.h>
#include <driver/driver.h>
#include <driver/of.h>
#include <errno.h>

#define KLOG_TAG "CPU_ARM"
#include "klog.h"

uint64_t cpu_arm_hw_id_get(struct cpudev *cpu)
{
    struct arm_cpu_priv *priv = (struct arm_cpu_priv *)dev_priv_get(cpu);

    return priv->affinity;
}

void cpu_arm_idle(struct cpudev *cpu)
{
    asm volatile("wfi");
}

int cpu_arm_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    static uint32_t cpu_index = 0;
    struct cpudev *cpu;
    struct cpu_dev_ops *cpu_ops;
    struct arm_cpu_priv *priv;

    if (of_property_match_string(dev->of_node, "device_type", "cpu") < 0)
    {
        return -EINVAL;
    }

    cpu = (struct cpudev *)memalign(8, sizeof(struct cpudev));
    if (!cpu)
    {
        return -ENOMEM;
    }
    memset(cpu, 0, sizeof(*cpu));

    priv = (struct arm_cpu_priv *)memalign(8, sizeof(struct arm_cpu_priv));
    if (!cpu)
    {
        return -ENOMEM;
    }
    memset(priv, 0, sizeof(*priv));

    dev_priv_set(dev, cpu);
    dev_priv_set(cpu, priv);

    cpu->index = cpu_index++;

    cpu_ops = (struct cpu_dev_ops *)dev->driver->of_match_table->data;
    if (cpu_ops && cpu_ops->cpu_affinity_set)
    {
        cpu_ops->cpu_affinity_set(dev);
    }
    cpu->dev_ops = cpu_ops;

    cpu->name = dev->driver->name;

    cpu_add_to_list(cpu);

    cpu_method_ops_match(dev);

    KLOG_I("cpu[%d] is boot cpu: %s", cpu->index, cpu->boot_cpu ? "ture" : "flase");

    return 0;
}
