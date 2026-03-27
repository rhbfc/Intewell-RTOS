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

#include <driver/cpu/cpu_arm.h>
#include <driver/cpudev.h>
#include <driver/devbus.h>
#include <driver/driver.h>
#include <driver/of.h>
#include <driver/platform.h>
#include <ttos_init.h>

static int cortex_a55_affinity_set(struct device *dev)
{
    struct cpudev *cpu;
    struct arm_cpu_priv *priv;

    cpu = (struct cpudev *)dev_priv_get(dev);
    priv = (struct arm_cpu_priv *)dev_priv_get(cpu);

    cpu_armv8_affinity_set(dev);

    cpu->code_id = (priv->affinity >> 8) & 0xFF;
    cpu->cluster_id = (priv->affinity >> 16) & 0xFF;

    return 0;
}

static uint64_t cortex_a55_hw_id_get(struct cpudev *cpu)
{
    struct arm_cpu_priv *priv = (struct arm_cpu_priv *)dev_priv_get(cpu);

    return priv->affinity;
}

static struct cpu_dev_ops cortex_a55_ops = {
    .cpu_affinity_set = cortex_a55_affinity_set,
    .cpu_is_self = cpu_armv8_is_self,
    .cpu_hw_id_get = cpu_arm_hw_id_get,
    .cpu_idle = cpu_arm_idle,
};

static struct of_device_id cpu_table[] = {
    {.compatible = "arm,cortex-a55", .data = (void *)&cortex_a55_ops},
    {/* end of list */},
};

static struct platform_driver cortex_a55_driver = {
    .probe = cpu_arm_probe,
    .driver =
        {
            .name = "ARMv8 Cortex-A55",
            .of_match_table = cpu_table,
        },
};

static int32_t cortex_a55_driver_init(void)
{
    return platform_add_driver(&cortex_a55_driver);
}
INIT_EXPORT_PRE_CPU(cortex_a55_driver_init, "Cortex A55 driver");
