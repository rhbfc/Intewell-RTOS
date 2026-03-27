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
#include <driver/devbus.h>
#include <driver/driver.h>
#include <driver/of.h>
#include <driver/platform.h>
#include <ttos_init.h>

#define KLOG_TAG "CPU_ARM"
#include "klog.h"

bool cpu_armv7_is_self(struct cpudev *cpu)
{
    uint32_t reg;
    struct arm_cpu_priv *priv;

    priv = (struct arm_cpu_priv *)dev_priv_get(cpu);

    reg = sysreg_read(MPIDR);

    if (priv->affinity == (uint64_t)(reg & 0XFFFFFFull))
    {
        return true;
    }

    return false;
}

int cpu_armv7_affinity_set(struct device *dev)
{
    uint32_t reg;
    uint32_t address_cells;
    uint32_t value[2];
    struct cpudev *cpu;
    struct arm_cpu_priv *priv;

    cpu = (struct cpudev *)dev_priv_get(dev);
    priv = (struct arm_cpu_priv *)dev_priv_get(cpu);

    /* 对于32位ARMv7或更高版本系统, reg与CPU MPIDR[23:0]寄存器为匹配 */
    if (of_property_read_u32(dev->of_node->parent, "#address-cells", &address_cells))
    {
        return -EINVAL;
    }

    if ((address_cells > 2) || (address_cells == 0))
    {
        return -EINVAL;
    }

    of_property_read_u32_array(dev->of_node, "reg", value, address_cells);

    if (address_cells == 2)
        priv->affinity = (uint64_t)((uint64_t)(value[0] & 0xFF) << 32) | (value[1] & 0xFFFFFF);
    else
        priv->affinity = (uint64_t)(value[0] & 0xFFFFFF);

    reg = sysreg_read(MPIDR);
    if (priv->affinity == (uint64_t)(reg & 0XFFFFFFull))
    {
        cpu->boot_cpu = true;
    }

    cpu->code_id = priv->affinity & 0x3;
    cpu->cluster_id = (priv->affinity >> 8) & 0xF;

    return 0;
}

uint64_t cpu_armv7_hw_id_get(struct cpudev *cpu)
{
    struct arm_cpu_priv *priv = (struct arm_cpu_priv *)dev_priv_get(cpu);

    return priv->affinity;
}

static struct cpu_dev_ops cpu_armv7_ops = {
    .cpu_affinity_set = cpu_armv7_affinity_set,
    .cpu_is_self = cpu_armv7_is_self,
    .cpu_hw_id_get = cpu_arm_hw_id_get,
    .cpu_idle = cpu_arm_idle,
};

static struct of_device_id cpu_table[] = {
    {.compatible = "arm,armv7", .data = (void *)&cpu_armv7_ops},
    {.compatible = "arm,cortex-a7", .data = (void *)&cpu_armv7_ops},
    {.compatible = "arm,cortex-a15", .data = (void *)&cpu_armv7_ops},
    {/* end of list */},
};

static struct platform_driver cpu_armv7_driver = {.probe = cpu_arm_probe,
                                                  .driver = {
                                                      .name = "ARMv7 Common",
                                                      .of_match_table = cpu_table,
                                                  }};

static int32_t cpu_armv7_driver_init(void)
{
    return platform_add_driver(&cpu_armv7_driver);
}
INIT_EXPORT_PRE_CPU(cpu_armv7_driver_init, "ARMv7 Common driver");
