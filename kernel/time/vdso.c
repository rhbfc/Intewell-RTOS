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

#include <barrier.h>
#include <cache.h>
#include <errno.h>
#include <page.h>
#include <spinlock.h>
#include <string.h>
#include <sys/mman.h>
#include <time/ktime.h>
#include <ttosMM.h>
#include <ttosProcess.h>
#include <ttos_init.h>
#include <ttos_time.h>
#include <vdso_datapage.h>

#define KLOG_TAG "vdso"
#include <klog.h>

extern const unsigned char __ttos_vdso_image_start[];
extern const unsigned char __ttos_vdso_image_end[];
extern unsigned char __ttos_vdso_vvar_start[];
extern uint64_t tsc_freq_get(void);
extern unsigned long long arch_vdso_freq_get (void);

struct vdso_image
{
    phys_addr_t vvar_pa;
    phys_addr_t image_pa;
    size_t image_pages;
    int ready;
    ttos_spinlock_t lock;
};

static struct vdso_image g_vdso = {0};

virt_addr_t arch_vdso_image_vaddr_get(void)
{
    return (virt_addr_t)__ttos_vdso_image_start;
}

phys_addr_t arch_vdso_image_paddr_get(void)
{
    return mm_kernel_v2p((virt_addr_t)__ttos_vdso_image_start);
}

static phys_addr_t arch_vdso_vvar_paddr_get(void)
{
    return mm_kernel_v2p((virt_addr_t)__ttos_vdso_vvar_start);
}

void vdso_update_data(void)
{
    irq_flags_t flags;
    virt_addr_t kaddr;
    ktime_t offs_real;
    ktime_t offs_boot;
    ktime_t offs_tai;
    struct ttos_vdso_data *data;

    if (!g_vdso.ready)
    {
        return;
    }

    kaddr = ktmp_access_start(g_vdso.vvar_pa);
    data = (struct ttos_vdso_data *)kaddr;

    ktime_get_update_offsets(&offs_real, &offs_boot, &offs_tai);
    (void)offs_boot;
    (void)offs_tai;

    smp_mb();

    if (!data->cycle_freq)
    {
        data->cycle_freq   = arch_vdso_freq_get();
    }

    if (!data->clock_res_ns)
    {
        data->clock_res_ns = data->cycle_freq ? (NSEC_PER_SEC / data->cycle_freq) : 1ULL;
    }

    spin_lock_irqsave(&g_vdso.lock, flags);
    data->realtime_offset_ns = offs_real;
    spin_unlock_irqrestore(&g_vdso.lock, flags);

    smp_mb();

    cache_dcache_flush(kaddr, PAGE_SIZE);
    ktmp_access_end(kaddr);
}


/*
用户虚拟地址 (VA, 高 -> 低)

高地址
┌──────────────────────────────┐
│ vDSO image page N-1          │
├──────────────────────────────┤
│ ...                          │
├──────────────────────────────┤
│ vDSO image page 1            │
├──────────────────────────────┤
│ vDSO image page 0            │
│ 区间: [vdso_start, vdso_start + N * PAGE_SIZE)
│ N = g_vdso.image_pages
└──────────────────────────────┘
┌──────────────────────────────┐
│ vvar (1 page)                │
│ 区间: [vvar_start, vvar_start + PAGE_SIZE)
└──────────────────────────────┘
低地址

其中: vdso_start = vvar_start + PAGE_SIZE
*/
virt_addr_t vdso_map(pcb_t pcb)
{
    virt_addr_t vvar_start = 0;
    virt_addr_t vdso_start;
    phys_addr_t pa;

    if (!g_vdso.ready)
    {
        return 0;
    }

    pa = process_mm_map(pcb, g_vdso.vvar_pa, &vvar_start, MT_USER, 1, MAP_SHARED);
    if (!pa)
    {
        return 0;
    }

    vdso_start = vvar_start + PAGE_SIZE;
    pa = process_mm_map(pcb, g_vdso.image_pa, &vdso_start, MT_CODE | MT_USER, g_vdso.image_pages,
                        MAP_SHARED|MAP_FIXED);
    if (!pa)
    {
        (void)process_mm_unmap(pcb, vvar_start);
        return 0;
    }

    return vdso_start;
}

static int vdso_init(void)
{
    size_t vdso_size = (size_t)(__ttos_vdso_image_end - __ttos_vdso_image_start);
    virt_addr_t kaddr;

    if (vdso_size == 0)
    {
        KLOG_EMERG("vdso image is empty");
        return -EINVAL;
    }

    g_vdso.image_pages = PAGE_ALIGN(vdso_size) >> PAGE_SIZE_SHIFT;
    g_vdso.image_pa = arch_vdso_image_paddr_get();
    if (!g_vdso.image_pa)
    {
        KLOG_EMERG("translate vdso image address failed");
        return -EINVAL;
    }

    g_vdso.vvar_pa = arch_vdso_vvar_paddr_get();
    if (!g_vdso.vvar_pa)
    {
        KLOG_EMERG("translate vvar address failed");
        return -EINVAL;
    }

    kaddr = ktmp_access_start(g_vdso.vvar_pa);
    memset((void *)kaddr, 0, PAGE_SIZE);
    cache_dcache_flush(kaddr, PAGE_SIZE);
    ktmp_access_end(kaddr);

    spin_lock_init(&g_vdso.lock);
    g_vdso.ready = 1;

    vdso_update_data();

    return 0;
}

#ifdef CONFIG_VDSO
INIT_EXPORT_COMPONENTS(vdso_init, "vdso init");
#endif
