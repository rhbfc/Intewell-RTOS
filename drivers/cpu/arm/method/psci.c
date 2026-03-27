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
#include <driver/of.h>
#include <driver/platform.h>
#include <errno.h>
#include <hvc.h>
#include <psci.h>
#include <smc.h>

#define KLOG_TAG "PSCI"
#include "klog.h"

typedef struct psci_ops
{
    struct cpu_ops ops;
    uint32_t (*version_get)(struct cpudev *cpu);
} psci_ops_t;

struct psci_method_list
{
    char *compatible;
    const psci_ops_t *ops;
};

static const psci_ops_t psci10_ops;
static const psci_ops_t psci02_ops;
static const psci_ops_t psci01_ops;

/* PSCI V0.2 */
static unsigned int psci02_version_get(struct cpudev *cpu)
{
    return cpu->method.psci_func((size_t)PSCI_VERSION, 0, 0, 0);
}

static int psci02_cpu_on(struct cpudev *cpu, phys_addr_t entry, unsigned long long context_id)
{
    struct arm_cpu_priv *priv = (struct arm_cpu_priv *)dev_priv_get(cpu);

    cpu->method.psci_func((size_t)PSCI_CPU_ON, (size_t)priv->affinity, (size_t)entry, context_id);
    return 0;
}

static int psci02_cpu_off(struct cpudev *cpu)
{
    cpu->method.psci_func((size_t)PSCI_CPU_OFF, 0, 0, 0);
    return 0;
}

static int psci02_cpu_suspend(struct cpudev *cpu, unsigned int power_state, phys_addr_t entry,
                              unsigned long long context_id)
{
    cpu->method.psci_func((size_t)PSCI_CPU_SUSPEND, power_state, entry, context_id);
    return 0;
}

static int psci02_cpu_migrate(struct cpudev *cpu, phys_addr_t entry, unsigned long long context_id)
{
    struct arm_cpu_priv *priv = (struct arm_cpu_priv *)dev_priv_get(cpu);

    cpu->method.psci_func((size_t)PSCI_MIGRATE, (size_t)priv->affinity, (size_t)entry, context_id);
    return 0;
}

static int psci02_cpu_reset(struct cpudev *cpu)
{
    cpu->method.psci_func((size_t)PSCI_SYSTEM_RESET, 0, 0, 0);
    return 0;
}

static int psci02_system_poweroff(struct cpudev *cpu)
{
    cpu->method.psci_func((size_t)PSCI_SYSTEM_OFF, 0, 0, 0);
    return 0;
}

/* PSCI V0.1 */
static int psci01_cpu_on(struct cpudev *cpu, phys_addr_t entry, unsigned long long context_id)
{
    struct arm_cpu_priv *priv = (struct arm_cpu_priv *)dev_priv_get(cpu);

    cpu->method.psci_func((size_t)cpu->method.psci01_code[PSCI01_ON], (size_t)priv->affinity,
                          (size_t)entry, context_id);
    return 0;
}

static int psci01_cpu_off(struct cpudev *cpu)
{
    cpu->method.psci_func((size_t)cpu->method.psci01_code[PSCI01_OFF], 0, 0, 0);
    return 0;
}

static int psci01_cpu_suspend(struct cpudev *cpu, unsigned int power_state, phys_addr_t entry,
                              unsigned long long context_id)
{
    cpu->method.psci_func((size_t)cpu->method.psci01_code[PSCI01_SUSPEND], power_state, entry,
                          context_id);
    return 0;
}

static int psci01_cpu_migrate(struct cpudev *cpu, phys_addr_t entry, unsigned long long context_id)
{
    struct arm_cpu_priv *priv = (struct arm_cpu_priv *)dev_priv_get(cpu);

    cpu->method.psci_func((size_t)cpu->method.psci01_code[PSCI01_MIGRATE], (size_t)priv->affinity,
                          (size_t)entry, context_id);
    return 0;
}

static int psci_init(struct cpudev *cpu)
{
    psci_ops_t *psci_ops;
    unsigned int version;

    if (strcmp(cpu->method.ops->name, "psci-0.1") == 0)
    {
        if (cpu->method.psci01_code[PSCI01_ON])
            return 0;

        cpu->method.ops = (struct cpu_ops *)&psci02_ops;
    }

    psci_ops = (psci_ops_t *)cpu->method.ops;

    version = psci_ops->version_get(cpu);
    KLOG_I("PSCI version: v%d.%d", PSCI_MAJOR_VERSION(version), PSCI_MINOR_VERSION(version));

    switch (version)
    {
    case PSCI_MAKE_VERSION(0, 2):
        cpu->method.ops = (struct cpu_ops *)&psci02_ops;
        break;

    case PSCI_MAKE_VERSION(1, 0):
        cpu->method.ops = (struct cpu_ops *)&psci10_ops;
        break;

    default:
        break;
    }

    return 0;
}

static const psci_ops_t psci10_ops = {
    .ops.name = "psci-1.0",
    .ops.init = psci_init,
    .ops.cpu_on = psci02_cpu_on,
    .ops.cpu_off = psci02_cpu_off,
    .ops.cpu_suspend = psci02_cpu_suspend,
    .ops.cpu_migrate = psci02_cpu_migrate,
    .ops.cpu_reset = psci02_cpu_reset,
    .ops.system_poweroff = psci02_system_poweroff,
    .version_get = psci02_version_get,
};

static const psci_ops_t psci02_ops = {
    .ops.name = "psci-0.2",
    .ops.init = psci_init,
    .ops.cpu_on = psci02_cpu_on,
    .ops.cpu_off = psci02_cpu_off,
    .ops.cpu_suspend = psci02_cpu_suspend,
    .ops.cpu_migrate = psci02_cpu_migrate,
    .ops.cpu_reset = psci02_cpu_reset,
    .ops.system_poweroff = psci02_system_poweroff,
    .version_get = psci02_version_get,
};

static const psci_ops_t psci01_ops = {
    .ops.name = "psci-0.1",
    .ops.init = psci_init,
    .ops.cpu_on = psci01_cpu_on,
    .ops.cpu_off = psci01_cpu_off,
    .ops.cpu_suspend = psci01_cpu_suspend,
    .ops.cpu_reset = NULL,
    .ops.cpu_migrate = psci01_cpu_migrate,
};

static struct psci_method_list psci_method_arry[] = {
    {.compatible = "arm,psci-1.0", .ops = &psci10_ops},
    {.compatible = "arm,psci-0.2", .ops = &psci02_ops},
    {.compatible = "arm,psci", .ops = &psci01_ops},
    {.compatible = "psci-1.0", .ops = &psci10_ops},
    {.compatible = "psci-0.2", .ops = &psci02_ops},
    {.compatible = "psci", .ops = &psci01_ops},
};

int psci_method_init(struct cpudev *cpu)
{
    struct device *psci_dev;
    int i;

    if (!cpu->boot_cpu && !cpu_method_copy(cpu))
    {
        KLOG_I("cpu[%d] enable-method:{%s}", cpu->index, cpu->method.ops->name);
        return 0;
    }

    psci_dev = platform_bus_find_device_byname("psci");
    if (psci_dev == NULL)
    {
        return -ENODEV;
    }

    for (i = 0; i < sizeof(psci_method_arry) / sizeof(struct psci_method_list); i++)
    {
        if (!of_property_match_string(psci_dev->of_node, "compatible",
                                      psci_method_arry[i].compatible))
        {
            cpu->method.ops = (struct cpu_ops *)psci_method_arry[i].ops;
            break;
        }
    }

    if (!of_property_match_string(psci_dev->of_node, "method", "hvc"))
    {
        cpu->method.psci_func = hvc_call;
    }
    else
    {
        cpu->method.psci_func = smc_call;
    }

    if (cpu->method.ops && cpu->method.ops->init)
        cpu->method.ops->init(cpu);

    return 0;
}
