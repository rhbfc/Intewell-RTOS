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

#include <driver/cpudev.h>
#include <errno.h>
#include <io.h>
#include <string.h>
#include <ttosMM.h>

#define KLOG_TAG "spin-table"
#include "klog.h"

static int spin_cpu_on(struct cpudev *cpu, phys_addr_t entry, unsigned long long context_id)
{
    virt_addr_t entry_virt;

    if (!cpu->release_addr)
    {
        return -EIO;
    }

    entry_virt = (virt_addr_t)ioremap(cpu->release_addr, sizeof(cpu->release_addr));

    writel(entry, entry_virt);
    barrier();

    iounmap(entry_virt);

    return 0;
}

static const struct cpu_ops spin_ops = {
    .name = "spin-table",
    .cpu_on = spin_cpu_on,
};

int spin_method_init(struct cpudev *cpu)
{
    cpu->method.ops = (struct cpu_ops *)&spin_ops;

    return 0;
}
