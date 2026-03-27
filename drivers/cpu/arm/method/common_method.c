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

#define KLOG_TAG "CPU-Method"
#include <klog.h>

static struct cpu_ops psci_method_ops = {
    .init = psci_method_init,
};

static struct cpu_ops spin_method_ops = {
    .init = spin_method_init,
};

static struct cpu_ops fmsh_method_ops = {
    .init = fmsh_method_init,
};

void cpu_method_ops_match(struct device *dev)
{
    struct cpudev *cpu;
    const char *string;
    uint32_t address_cells;
    uint32_t value[2];
    uint32_t i;

    cpu = (struct cpudev *)dev_priv_get(dev);
    cpu->method.ops = NULL;

    if (of_property_read_string(dev->of_node, "enable-method", &string) == 0)
    {
        if (strcmp(string, "psci") == 0)
        {
            cpu->method.ops = &psci_method_ops;
        }
        else if (strcmp(string, "spin-table") == 0)
        {
            if (of_property_read_u32(dev->of_node->parent, "#address-cells", &address_cells) == 0)
            {
                cpu->method.ops = &spin_method_ops;

                if (address_cells > 2)
                {
                    KLOG_E("cpu address_cells size error.");
                    return;
                }

                of_property_read_u32_array(dev->of_node, "reg", value, address_cells);

                for (i = 0; i < address_cells; i++)
                {
                    cpu->release_addr <<= 32;
                    cpu->release_addr |= value[i];
                }
            }
        }
    }
    else
    {
        if (of_property_match_string(of_get_root_node(), "compatible", "fmsh,fmsh-psoc") >= 0)
        {
            cpu->method.ops = &fmsh_method_ops;
        }
    }

    if (cpu->method.ops && cpu->method.ops->init)
    {
        cpu->method.ops->init(cpu);
    }
}