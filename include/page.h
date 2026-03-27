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

#ifndef __PAGE_H__
#define __PAGE_H__

#include <list.h>
#include <mmu.h>
#include <spinlock.h>
#include <stddef.h>
#include <stdint.h>

#ifdef CONFIG_SLUB
struct kmem_cache;
#endif

struct page
{
    union {
        struct
        {
            struct page *next; /* same level next */
            struct page *pre;  /* same level pre  */
        };
        uintptr_t partial_next;
    };
    uint32_t size_bits; /* if is ARCH_ADDRESS_WIDTH_BITS, means not free */
    int ref_cnt;        /* page group ref count */

#ifdef CONFIG_SLUB
    unsigned long flags;              /* 页标志位 */
    struct kmem_cache *slab_cache;    /* 指向所属的kmem_cache */
    struct kmem_cache_cpu *cpu_cache; /* 指向所属的percpu_cache */
    atomic_t freelist;    /* 空闲对象链表头（带版本号的 tagged pointer） */
    atomic_t inuse;       /* 已使用对象数量 */
    atomic_t slab_state;  /* 页状态（防止重复入队）*/
    unsigned int objects; /* 总对象数量 */
#endif
};

typedef struct tag_region
{
    uintptr_t start;
    uintptr_t end;
} region_t;

#define PFN_UP(addr) ((pfn_t)(((addr) + PAGE_SIZE - 1) >> PAGE_SIZE_SHIFT))
#define PFN_DOWN(addr) ((pfn_t)((addr) >> PAGE_SIZE_SHIFT))

phys_addr_t page_to_addr(struct page *p);
struct page *addr_to_page(phys_addr_t addr);
struct page *ptr_to_page(void *vaddr);

/**
 * @brief init page allocator
 *
 * @param reg
 */
void page_init(region_t reg);

/**
 * @brief alloc page
 *
 * @param size_bits
 * @return void*
 */
phys_addr_t pages_alloc(uint32_t size_bits, int zone);

struct page *alloc_page(void);
void free_page(struct page *p);

/**
 * @brief increase ref count
 *
 * @param addr
 * @param size_bits
 */
void page_ref_inc(phys_addr_t addr, uint32_t size_bits);

/**
 * @brief get ref count
 *
 * @param addr
 * @param size_bits
 * @return int
 */
int page_ref_get(phys_addr_t addr, uint32_t size_bits);

/**
 * @brief free page
 *
 * @param addr
 * @param size_bits
 * @return int
 */
int pages_free(phys_addr_t addr, uint32_t size_bits);

/**
 * @brief dump page allocator info
 *
 */
void pageinfo_dump(void);

/**
 * @brief convert size to page bit
 *
 * @param size
 * @return uintptr_t
 */
uint32_t page_bits(size_t size);

/**
 * @brief get page info
 *
 * @param total_nr
 * @param free_nr
 */
void page_get_info(uintptr_t *total_nr, uintptr_t *free_nr);

pfn_t page_to_pfn(struct page *p);
struct page *pfn_to_page(pfn_t pfn);
void set_page_reserved(phys_addr_t start, phys_addr_t end);
void set_page_free(phys_addr_t start, int order);
void totalram_pages_add(int count);
void page_mem_regions_set(phys_addr_t addr, phys_addr_t size);

virt_addr_t page_address(phys_addr_t addr);

#endif /* __PAGE_H__ */
