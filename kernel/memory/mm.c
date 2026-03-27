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

#include "page.h"
#include <assert.h>
#include <commonTypes.h>
#include <ttosMM.h>
#include <memblock.h>
#include <system/kconfig.h>

#define KLOG_TAG "MM"
#include <klog.h>

extern int    __end__;
extern int    __executable_start;
extern size_t kernel_mmu_get_page_size (void);
extern int    kernel_mmu_init (void);
static bool g_page_allocer_inited = false;

void multiboot_init (void);
void multiboot_mem_scan (void (*mem_add_func)(phys_addr_t addr, phys_addr_t size, void *ctx), 
                        void (*mem_reserve_func)(phys_addr_t addr, phys_addr_t size, void *ctx));

int fdt_foreach_memory(void (*func)(phys_addr_t addr, phys_addr_t size, void *ctx), void *ctx);
int fdt_foreach_memory_reserved(void (*func)(phys_addr_t addr, phys_addr_t size, void *ctx), void *ctx);

static void mem_enum (phys_addr_t addr, phys_addr_t size, void *ctx)
{
    ttos_memblk_add(addr, size);
    page_mem_regions_set(addr, size);
}

static void mem_enum_reserved (phys_addr_t addr, phys_addr_t size, void *ctx)
{
    ttos_memblk_reserve(addr, size);
}

bool page_allocer_inited(void)
{
    return g_page_allocer_inited;
}
/**
 * @brief 内存管理器初始化
 *
 * @return INT32_T 0: 初始化完成 其他: 错误
 */
int32_t ttosMemoryManagerInit (void)
{
    phys_addr_t start, size;
        /* arm架构下earlycon初始化通过设备树解析得到基地址，x86跳过这一步 */
    if(!IS_ENABLED(__x86_64__))
    {
        fdt_foreach_memory(mem_enum, NULL);
        fdt_foreach_memory_reserved(mem_enum_reserved, NULL);
    }
    else
    {
        multiboot_init ();
        multiboot_mem_scan (mem_enum, mem_enum_reserved);
    }

    start = (phys_addr_t)(virt_addr_t)&__executable_start + get_kernel_mm()->pv_offset;
    size = (virt_addr_t)&__end__ - (virt_addr_t)&__executable_start;
    size = ALIGN_UP(size, PAGE_SIZE);
    ttos_memblk_reserve (start, size);

    start = (phys_addr_t)(virt_addr_t)KERNEL_SPACE_START + get_kernel_mm()->pv_offset;
    ttos_memblk_reserve (start, 0x80000);

    if(IS_ENABLED(CONFIG_X86_64))
    {
        void memblk_dump_all (void);
        memblk_dump_all ();
    }
    kernel_mmu_init ();
    ttos_memblk_release_mem_to_pagealloc();
    g_page_allocer_inited = true;
    return 0;
}

T_ULONG ttosGetPageSize (T_VOID)
{
    return kernel_mmu_get_page_size ();
}

struct page_obj *ttosPageAllocObj (T_UWORD count)
{
    struct page_obj *obj = malloc (sizeof (struct page_obj));
    if (obj == NULL)
    {
        return NULL;
    }
    obj->page_count = count;
    obj->page_start = page_address(pages_alloc(page_bits(count << PAGE_SIZE_SHIFT), ZONE_NORMAL));
    if (obj->page_start == 0)
    {
        free (obj);
        return NULL;
    }
    return obj;
}
int32_t ttosPageGetInfo (uintptr_t *total_nr, uintptr_t *free_nr)
{
    page_get_info (total_nr, free_nr);
    return 0;
}
