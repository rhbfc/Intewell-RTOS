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

#include "ttos_init.h"
#include "percpu.h"
#include "cache.h"
#include "string.h"
#include <ttosMM.h>
#include <page.h>

unsigned long __per_cpu_offset[CONFIG_MAX_CPUS] = { 0 };

static int percpu_init(void)
{
    uintptr_t addr;
    uint32_t i;

    for (i = 0; i < CONFIG_MAX_CPUS; i++)
    {
        addr = page_address(pages_alloc( page_bits(PAGE_ALIGN(PER_CPU_SIZE)), ZONE_NORMAL));
        if (addr == 0)
            return -1;

        __per_cpu_offset[i] = addr - (uintptr_t) PER_CPU_START_VADDR;

        memcpy((void *) addr, PER_CPU_START_VADDR, PER_CPU_SIZE);
        cache_dcache_clean(addr, PER_CPU_SIZE);
    }

    return 0;
}
INIT_EXPORT_PRE(percpu_init, "percpu init");