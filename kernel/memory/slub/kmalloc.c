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

#include <kmalloc.h>
#include <page.h>
#include <slub.h>
#include <stdio.h>
#include <string.h>
#include <ttosMM.h>
#include <ttosUtils.h>
#include <ttos_init.h>

#define KLOG_TAG "slub"
#include <klog.h>

struct kmem_cache kmalloc_caches[9] = {0};

int __kernel_malloc_init(void)
{
    int i;
    char name[16];
    for (i = 0; i < 9; i++)
    {
        snprintf(name, sizeof(name), "slub-%d", 8 << i);
        kmem_cache_init(&kmalloc_caches[i], name, 8 << i, 0, 0, NULL);
    }
    return 0;
}
INIT_EXPORT_PRE(__kernel_malloc_init, "init malloc");

void *kmalloc(size_t size, gfp_t flags)
{
    int i;
    if (size == 0)
    {
        return NULL;
    }
    /* 如果可以使用slub分配器 */
    if (size < (8 << 8))
    {
        for (i = 0; i < 9; i++)
        {
            if (size <= (8 << i))
            {
                return kmem_cache_alloc(&kmalloc_caches[i], flags);
            }
        }
        return NULL;
    }
    /* 大对象分配 */
    int order = page_bits(size);

    phys_addr_t addr = pages_alloc(order, ZONE_NORMAL);
    if (addr == 0)
    {
        return NULL;
    }

    struct page *page = addr_to_page(addr);
    if (!page)
    {
        return NULL;
    }

    page->flags = PG_kmalloc;
    PG_order_set(page, order);
    page->objects = 1;
    atomic_write(&page->inuse, 1);

    return (void *)page_address(addr);
}

void kfree(void *ptr)
{
    if (ptr == NULL)
    {
        return;
    }
    struct page *page = ptr_to_page(ptr);
    if (!page)
    {
        KLOG_E("free null point");
        return;
    }
    if (page->flags & PG_kmalloc)
    {
        uint32_t order = PG_order_get(page);
        page->flags = 0;
        pages_free(page_to_addr(page), order);
        return;
    }
    if (page->flags & PG_slab)
    {
        struct kmem_cache *cache = page->slab_cache;
        if (!cache)
        {
            KLOG_E("not found memcache");
            return;
        }
        kmem_cache_free(cache, ptr);
        return;
    }
    KLOG_E("not found allocer");
}

size_t ksize(void *ptr)
{
    if (ptr == NULL)
    {
        KLOG_E("ksize null point");
        return 0;
    }
    struct page *page = ptr_to_page(ptr);
    if (!page)
    {
        KLOG_E("ksize error point");
        return 0;
    }

    if (page->flags & PG_kmalloc)
    {
        return (0x01 << (PG_order_get(page) + PAGE_SIZE_SHIFT));
    }
    if (page->flags & PG_slab)
    {
        struct kmem_cache *cache = page->slab_cache;
        if (!cache)
        {
            KLOG_E("not found memcache");
            return 0;
        }
        return cache->object_size;
    }
    KLOG_E("ksize error point");
    return 0;
}

void *kzalloc(size_t size, gfp_t flags)
{
    void *p = kmalloc(size, flags);
    memset(p, 0, size);
    return p;
}

void *krealloc(void *ptr, size_t size, gfp_t flags)
{
    if (ptr == NULL)
    {
        return kmalloc(size, flags);
    }

    if (size == 0)
    {
        kfree(ptr);
        return NULL;
    }

    size_t old_size = ksize(ptr);
    if (old_size >= size)
    {
        return ptr;
    }

    void *p = kmalloc(size, flags);
    if (p == NULL)
    {
        return NULL;
    }
    memcpy(p, ptr, old_size);
    kfree(ptr);
    return p;
}

static struct memory_info meminfo;

static void _get_malloc_info(struct memory_info *info)
{
    int cached, tatol, free;
    slub_get_info(&cached, &tatol, &free);
    info->total = (long)tatol << PAGE_SIZE_SHIFT;
    info->free = (long)free << PAGE_SIZE_SHIFT;
    info->used = (long)(info->total - info->free);
}

struct memory_info *get_malloc_info(void)
{
    _get_malloc_info(&meminfo);
    return &meminfo;
}
