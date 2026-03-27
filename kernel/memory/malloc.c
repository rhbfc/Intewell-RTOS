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
#include <kmalloc.h>
#include <malloc.h>
#include <spinlock.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <symtab.h>
#include <system/gfp_types.h>
#include <ttosMM.h>
#include <ttos_init.h>

#ifdef CONFIG_WORKSPACE
#include "workspace/ttosHeap.h"
#include <ttosBase.h>
#include <ttosTypes.h>

/* 工作空间控制变量 */
T_TTOS_HeapControl g_kernel_hcb;
T_TTOS_HeapControl g_dma_hcb;

typedef T_TTOS_HeapControl *heap_t;
#elif defined(CONFIG_TLSF)
#include "tlsf/tlsf.h"
typedef tlsf_t heap_t;

#elif defined(CONFIG_SLUB)
#include <slub.h>
#endif /* CONFIG_SLUB */

static void *__kernel_malloc_impl(size_t size);
static void *__dma_malloc_impl(size_t size);
static void *__dma_aligned_alloc_impl(size_t align, size_t len);
static void __kernel_free_impl(void *p);
static void *__kernel_aligned_alloc_impl(size_t align, size_t len);
static void *__kernel_calloc_impl(size_t m, size_t n);
static void *__kernel_realloc_impl(void *p, size_t n);
static void *__kernel_zalloc_impl(size_t n);

KSYM_EXPORT_ALIAS(__kernel_malloc_impl, malloc);
KSYM_EXPORT_ALIAS(__dma_malloc_impl, dma_malloc);
KSYM_EXPORT_ALIAS(__dma_aligned_alloc_impl, dma_memalign);
KSYM_EXPORT_ALIAS(__kernel_free_impl, free);
KSYM_EXPORT_ALIAS(__kernel_aligned_alloc_impl, memalign);
KSYM_EXPORT_ALIAS(__kernel_calloc_impl, calloc);
KSYM_EXPORT_ALIAS(__kernel_realloc_impl, realloc);
KSYM_EXPORT_ALIAS(__kernel_zalloc_impl, zalloc);

#ifndef CONFIG_SLUB

#define MAX_HEAP 2

#define KERNEL_HEAP_INDEX 0
#define DMA_HEAP_INDEX 1

struct heap_node
{
    uintptr_t start;
    uintptr_t end;
    gfp_t type;
    heap_t handler;
    struct mm_region region;
} g_heap_table[MAX_HEAP];

#define kernel_heap ((heap_t)g_heap_table[KERNEL_HEAP_INDEX].handler)
#define dma_heap ((heap_t)g_heap_table[DMA_HEAP_INDEX].handler)

static DEFINE_SPINLOCK(g_malloc_lock);

static heap_t ptr2heap(void *ptr)
{
    int i;
    for (i = 0; i < MAX_HEAP; i++)
    {
        if ((uintptr_t)ptr < g_heap_table[i].end && (uintptr_t)ptr > g_heap_table[i].start)
        {
            return g_heap_table[i].handler;
        }
    }
    assert(0 && "ptr can not free");
    return NULL;
}

size_t __kernel_heap_block_size(void *p)
{
#ifdef CONFIG_WORKSPACE
    size_t size;
    ttosGetBlockSizeHeap(kernel_heap, p, &size);
    return size;
#elif defined(CONFIG_TLSF)
    return tlsf_block_size(p);
#endif
}

static void *__kernel_malloc_impl(size_t size)
{
    void *mem;
    uintptr_t flags;
    spin_lock_irqsave(&g_malloc_lock, flags);
#ifdef CONFIG_WORKSPACE
    ttosAllocateHeap(kernel_heap, size, 8, &mem);
#elif defined(CONFIG_TLSF)
    mem = tlsf_memalign(kernel_heap, 8, size);
#endif
    spin_unlock_irqrestore(&g_malloc_lock, flags);

    return mem;
}

static void *__dma_malloc_impl(size_t size)
{
    void *mem;
    uintptr_t flags;
    spin_lock_irqsave(&g_malloc_lock, flags);
#ifdef CONFIG_WORKSPACE
    ttosAllocateHeap(dma_heap, size, 8, &mem);
#elif defined(CONFIG_TLSF)
    mem = tlsf_memalign(dma_heap, 8, size);
#endif
    spin_unlock_irqrestore(&g_malloc_lock, flags);

    return mem;
}

static void *__dma_aligned_alloc_impl(size_t align, size_t len)
{
    void *mem;
    uintptr_t flags;
    spin_lock_irqsave(&g_malloc_lock, flags);
#ifdef CONFIG_WORKSPACE
    ttosAllocateHeap(dma_heap, len, align, &mem);
#elif defined(CONFIG_TLSF)
    mem = tlsf_memalign(dma_heap, align, len);
#endif
    spin_unlock_irqrestore(&g_malloc_lock, flags);
    return mem;
}

static void __kernel_free_impl(void *p)
{
    uintptr_t flags;

    spin_lock_irqsave(&g_malloc_lock, flags);
#ifdef CONFIG_WORKSPACE
    ttosFreeHeap(ptr2heap(p), p);
#elif defined(CONFIG_TLSF)
    tlsf_free(ptr2heap(p), p);
#endif
    spin_unlock_irqrestore(&g_malloc_lock, flags);
}

void kfree(void *ptr)
{
    __kernel_free_impl(ptr);
}

void *kmalloc(size_t size, gfp_t flags)
{
    return __kernel_malloc_impl(size);
}

static void *__kernel_aligned_alloc_impl(size_t align, size_t len)
{
    void *mem;
    uintptr_t flags;
    spin_lock_irqsave(&g_malloc_lock, flags);
#ifdef CONFIG_WORKSPACE
    ttosAllocateHeap(kernel_heap, len, align, &mem);
#elif defined(CONFIG_TLSF)
    mem = tlsf_memalign(kernel_heap, align, len);
#endif
    spin_unlock_irqrestore(&g_malloc_lock, flags);
    return mem;
}

static void *__kernel_calloc_impl(size_t m, size_t n)
{
    size_t s = n * m;
    void *p = malloc(s);
    if (p == NULL)
    {
        return NULL;
    }
    memset(p, 0, s);
    return p;
}

static void *__kernel_realloc_impl(void *p, size_t n)
{
    void *mem;

    if (p == NULL)
    {
        return __kernel_malloc_impl(n);
    }

#if defined(CONFIG_WORKSPACE)
    size_t old_size;

    old_size = __kernel_heap_block_size(p);

    if (n <= old_size)
    {
        return p;
    }

    mem = __kernel_malloc_impl(n);
    memcpy(mem, p, old_size);
    __kernel_free_impl(p);
#elif defined(CONFIG_TLSF)
    uintptr_t flags;
    spin_lock_irqsave(&g_malloc_lock, flags);
    mem = tlsf_realloc(kernel_heap, p, n);
    spin_unlock_irqrestore(&g_malloc_lock, flags);
#endif

    return mem;
}

static void *__kernel_zalloc_impl(size_t n)
{
    void *p = malloc(n);
    if (p == NULL)
    {
        return NULL;
    }
    memset(p, 0, n);
    return p;
}

void *kzalloc(size_t size, gfp_t flags)
{
    void *p = __kernel_zalloc_impl(size);
    return p;
}
static int __kernel_malloc_init(void)
{
    void *heapstart;
    size_t heapsize;

    heapstart = (void *)page_address(pages_alloc(page_bits(CONFIG_KERNEL_HEAP_SIZE), ZONE_NORMAL));
    heapsize = CONFIG_KERNEL_HEAP_SIZE;

    g_heap_table[KERNEL_HEAP_INDEX].start = (uintptr_t)heapstart;
    g_heap_table[KERNEL_HEAP_INDEX].end = (uintptr_t)heapstart + heapsize;
    g_heap_table[KERNEL_HEAP_INDEX].type = GFP_KERNEL;

#ifdef CONFIG_WORKSPACE
    g_heap_table[KERNEL_HEAP_INDEX].handler = &g_kernel_hcb;
    ttosInitHeap(kernel_heap, heapstart, heapsize);
#elif defined(CONFIG_TLSF)
    g_heap_table[KERNEL_HEAP_INDEX].handler = tlsf_create_with_pool(heapstart, heapsize);
#endif

    heapstart =
        (void *)page_address(pages_alloc(page_bits(CONFIG_KERNEL_NC_HEAP_SIZE), ZONE_NORMAL));
    heapsize = CONFIG_KERNEL_NC_HEAP_SIZE;

    ttosSetPageAttribute((virt_addr_t)heapstart, heapsize, MT_NCACHE | MT_KERNEL);

#ifdef CONFIG_WORKSPACE
    g_heap_table[DMA_HEAP_INDEX].handler = &g_dma_hcb;
    ttosInitHeap(dma_heap, heapstart, heapsize);
#elif defined(CONFIG_TLSF)
    g_heap_table[DMA_HEAP_INDEX].handler = tlsf_create_with_pool(heapstart, heapsize);
#endif
    return 0;
}
INIT_EXPORT_PRE(__kernel_malloc_init, "init malloc");

#if defined(CONFIG_TLSF)
static void __tlsf_walker(void *ptr, size_t size, int used, void *user)
{
    struct memory_info *info = (struct memory_info *)user;
    info->total += size;
    if (!used)
    {
        info->free += size;
    }
    else
    {
        info->used += size;
    }
}
#endif /* CONFIG_TLSF */
static struct memory_info meminfo;

static void _get_malloc_info(heap_t heap, struct memory_info *info)
{
#ifdef CONFIG_WORKSPACE
    info->free = heap->heap_size - heap->in_use_size;
    info->total = heap->heap_size;
    info->used = heap->in_use_size;
#elif defined(CONFIG_TLSF)
    info->free = 0;
    info->total = 0;
    info->used = 0;
    tlsf_walk_pool(tlsf_get_pool(heap), __tlsf_walker, info);
#endif
}

struct memory_info *get_malloc_info(void)
{
    _get_malloc_info(kernel_heap, &meminfo);
    return &meminfo;
}

#else

static void *__kernel_malloc_impl(size_t size)
{
    void *mem = kmalloc(size, GFP_KERNEL);
    return mem;
}

static void *__kernel_zalloc_impl(size_t n)
{
    void *mem = kzalloc(n, GFP_KERNEL);
    return mem;
}

static void *__dma_malloc_impl(size_t size)
{
    void *mem = kmalloc(size, GFP_DMA32);
    return mem;
}

static void *__dma_aligned_alloc_impl(size_t align, size_t len)
{
    /* slub 分配的内存天然是对齐的,依据C11标准len必须是 align的整数倍 所以这里必然是对齐的 */
    void *mem = kmalloc(len, GFP_DMA32);
    return mem;
}
static void __kernel_free_impl(void *p)
{
    kfree(p);
}
static void *__kernel_aligned_alloc_impl(size_t align, size_t len)
{
    /* slub 分配的内存天然是对齐的,依据C11标准len必须是 align的整数倍 所以这里必然是对齐的 */
    void *mem = kmalloc(len, GFP_KERNEL);
    return mem;
}
static void *__kernel_calloc_impl(size_t m, size_t n)
{
    // todo kmalloc_array
    void *mem = kzalloc(m * n, GFP_KERNEL);
    return mem;
}
static void *__kernel_realloc_impl(void *p, size_t n)
{
    void *mem = krealloc(p, n, GFP_KERNEL);
    return mem;
}

#endif

#if CONFIG_SHELL
#include <shell.h>
static void mem_size_dp(char *tag, unsigned long size)
{
    char s[] = {'K', 'M', 'G'};
    int i = 0;
    double ds = size;

    printf("%-5s: %15ld Bytes", tag, size);

    for (i = 0; i < 3; i++)
    {
        ds /= 1024;
        if (ds < 1024)
            break;
    }

    printf(" (%.3lf %cB)\n", ds, s[i]);
}

#ifndef CONFIG_SLUB

static void _free_dump(char *name, heap_t heap)
{
    struct memory_info meminfo;

    _get_malloc_info(heap, &meminfo);

    printf("|---------- %s memory info ----------|\n", name);
    mem_size_dp("Total", meminfo.total);
    mem_size_dp("Used", meminfo.used);
    mem_size_dp("Free", meminfo.free);
    printf("\n");
}

static int _free_cmd(int argc, const char *argv[])
{
    _free_dump("Kernel", kernel_heap);
    _free_dump("DMA", dma_heap);
    return 0;
}
#else
static int _free_cmd(int argc, const char *argv[])
{
    // slub_dump();
    return 0;
}
#endif
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) |
                     SHELL_CMD_DISABLE_RETURN,
                 free, _free_cmd, list heap free);
#endif /* CONFIG_SHELL */
