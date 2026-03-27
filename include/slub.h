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

#ifndef SLUB_H
#define SLUB_H

#include <barrier.h>
#include <list.h>
#include <spinlock.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <system/gfp_types.h>

/* 最大页数量（模拟物理内存） */
#define MAX_PAGES 65536

/* SLUB分配器的对象大小范围 */
#define MIN_OBJECT_SIZE 8
#define MAX_OBJECT_SIZE (PAGE_SIZE >> 1) /* 最大对象大小为页大小的一半 */

/* 每页最大对象数量 */
#define MAX_OBJECTS_PER_PAGE (PAGE_SIZE / MIN_OBJECT_SIZE)

/* 页标志位 */
#define PG_slab 0x01    /* 页属于SLUB分配器 */
#define PG_kmalloc 0x04 /* 页是由kmalloc分配的 */

#define PG_order_shift 0x08
#define PG_order_mask (0xff << PG_order_shift)
#define PG_order_set(page, order)                                                                  \
    do                                                                                             \
    {                                                                                              \
        (page)->flags |= ((order) << PG_order_shift);                                              \
    } while (0)
#define PG_order_get(page) (((page)->flags & PG_order_mask) >> PG_order_shift)

/*
 * Tagged pointer for ABA problem prevention
 *
 * 由于内核使用高位地址（高16位非零），改用低位存储 tag：
 * - 对象至少 8 字节对齐，低 3 位一定为 0
 * - 使用低 3 位存储 tag (0-7)，虽然范围小但配合状态机已足够
 *
 * 低3位: tag（版本号）
 * 高位: 指针（对齐部分）
 */
#define SLUB_TAG_BITS 3
#define SLUB_TAG_MASK ((1UL << SLUB_TAG_BITS) - 1) /* 0x7 */
#define SLUB_PTR_MASK (~SLUB_TAG_MASK)             /* ~0x7 */

/* 从 tagged pointer 中提取指针 */
static inline void *slub_ptr_untag(uintptr_t tagged)
{
    return (void *)(tagged & SLUB_PTR_MASK);
}

/* 创建 tagged pointer */
static inline uintptr_t slub_ptr_tag(void *ptr, unsigned int tag)
{
    return ((uintptr_t)ptr & SLUB_PTR_MASK) | (tag & SLUB_TAG_MASK);
}

/* 从 tagged pointer 中提取 tag */
static inline unsigned int slub_ptr_get_tag(uintptr_t tagged)
{
    return (unsigned int)(tagged & SLUB_TAG_MASK);
}

/* 增加 tag 并创建新的 tagged pointer */
static inline uintptr_t slub_ptr_inc_tag(void *ptr, uintptr_t old_tagged)
{
    unsigned int new_tag = (slub_ptr_get_tag(old_tagged) + 1) & SLUB_TAG_MASK;
    return slub_ptr_tag(ptr, new_tag);
}

/*
 * 页状态定义（用于防止重复入队）
 */
#define SLAB_PAGE_CPU_ACTIVE 0 /* 页是某个 CPU 的激活页 */
#define SLAB_PAGE_PARTIAL 1    /* 页在 partial 链表中 */
#define SLAB_PAGE_FULL 2       /* 页已满，不在任何链表中 */
#define SLAB_PAGE_FREE 3       /* 页将被释放 */

/* 前向声明 */
struct kmem_cache;
struct page;

/**
 * Per-CPU缓存结构体
 * 每个CPU都有独立的缓存，减少锁竞争
 */
struct kmem_cache_cpu
{
    void **freelist;     /* 当前CPU的空闲对象链表 */
    atomic_t free_rlist; /* 其他CPU归还的对象链表 */
    struct page *page;   /* 当前CPU的激活页 */
};

/**
 * SLUB缓存结构体 - 核心数据结构
 * 管理特定大小对象的分配
 */
struct kmem_cache
{
    /* 链表节点 */
    struct list_head list;

    struct kmem_cache_cpu __percpu[CONFIG_MAX_CPUS]; /* Per-CPU缓存数组 */

    /* 缓存属性 */
    unsigned int size;             /* 对象大小 */
    unsigned int object_size;      /* 实际对象大小（不包括元数据） */
    unsigned int offset;           /* 空闲指针偏移 */
    unsigned int objects_per_page; /* 每页对象数量 */

    /* 页管理 */
    atomic_t partial_list; /* 部分使用的页链表 */

    /* 统计信息 */
    atomic_t alloc_count;      /* 分配次数 */
    atomic_t slow_alloc_count; /* 分配次数 */
    atomic_t free_count;       /* 释放次数 */
    atomic_t page_count;       /* 使用的页数 */

    /* 缓存标识 */
    char name[32];      /* 缓存名称 */
    unsigned int flags; /* 缓存标志 */
    void (*ctor)(void *);
};

/**
 * SLUB分配器全局状态
 */
struct slub_allocator
{
    struct list_head cache_list; /* 缓存链表头 */
    ttos_spinlock_t cache_lock;  /* 缓存链表自旋锁 */
    bool initialized;            /* 初始化标志 */
};

/**
 * 函数声明
 */

/* 缓存管理 */
int kmem_cache_init(struct kmem_cache *cache, const char *name, size_t size, size_t align,
                    unsigned long flags, void (*ctor)(void *));
struct kmem_cache *kmem_cache_create(const char *name, size_t size, size_t align,
                                     unsigned long flags, void (*ctor)(void *));
void kmem_cache_destroy(struct kmem_cache *cache);
void *kmem_cache_alloc(struct kmem_cache *cache, gfp_t flags);
void kmem_cache_free(struct kmem_cache *cache, void *obj);

/* 工具函数 */
size_t kmem_cache_size(struct kmem_cache *cache);
void slub_print_stats(void);
void slub_get_info(int *total_caches, int *total_pages, int *free_pages);

#endif /* SLUB_H */
