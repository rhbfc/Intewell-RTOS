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

/**
 * @file memblock.h
 * @brief 启动早期物理内存区间管理接口。
 *
 * memblock 只负责启动早期的物理内存登记、保留和页级分配。
 * MMU 和页分配器初始化完成后，剩余物理页会交给常规页分配器管理。
 */
#ifndef _TTOS_MEMBLOCK_H
#define _TTOS_MEMBLOCK_H

#include <commonTypes.h>
#include <stdbool.h>
#include <system/types.h>

#define PHYS_ADDR_MAX (UINT64_MAX)

extern pfn_t min_low_pfn;
extern pfn_t max_low_pfn;
extern pfn_t max_pfn;
extern pfn_t max_possible_pfn;

/**
 * @brief memblock 区间属性。
 */
enum ttos_memblk_flags
{
    MEMBLOCK_NONE = 0x0,
    MEMBLOCK_NOMAP = 0x4,
};

/**
 * @brief 单个物理地址区间，表示半开区间 [start, start + size)。
 */
struct ttos_memblk_region
{
    phys_addr_t start;
    phys_addr_t size;
    enum ttos_memblk_flags flags;
};

/**
 * @brief 一类区间的集合。
 *
 * 当前实现只维护两类集合：可用区 ``memory`` 和保留区 ``reserved``。
 */
struct ttos_memblk_collect
{
    unsigned long rgn_num;
    unsigned long rgn_max;
    phys_addr_t total_size;
    struct ttos_memblk_region *regions;
    char *name;
};

/**
 * @brief 启动期 memblock 的全局状态。
 */
struct ttos_memblk
{
    int bottom_up;
    phys_addr_t current_limit;
    struct ttos_memblk_collect memory;
    struct ttos_memblk_collect reserved;
};

extern struct ttos_memblk memblock;

/**
 * @brief 将一段物理内存加入可用区。
 * @param start 区间起始物理地址。
 * @param size 区间大小，单位为字节。
 * @return 成功返回 0，失败返回负错误码。
 */
int ttos_memblk_add(phys_addr_t start, phys_addr_t size);
/**
 * @brief 将一段物理内存从可用区移除。
 * @param start 区间起始物理地址。
 * @param size 区间大小，单位为字节。
 * @return 成功返回 0，失败返回负错误码。
 */
int ttos_memblk_remove(phys_addr_t start, phys_addr_t size);
/**
 * @brief 将一段保留区释放回未保留状态。
 * @param start 区间起始物理地址。
 * @param size 区间大小，单位为字节。
 * @return 成功返回 0，失败返回负错误码。
 */
int ttos_memblk_free(phys_addr_t start, phys_addr_t size);
/**
 * @brief 将一段物理内存标记为保留。
 * @param start 区间起始物理地址。
 * @param size 区间大小，单位为字节。
 * @return 成功返回 0，失败返回负错误码。
 */
int ttos_memblk_reserve(phys_addr_t start, phys_addr_t size);

/**
 * @brief 为伙伴系统建立页描述符并交出未保留的页。
 * @return 当前实现固定返回 0。
 */
unsigned long ttos_memblk_release_mem_to_pagealloc(void);

/** @brief 分配接口使用的特殊地址边界值。 */
#define MEMBLOCK_ALLOC_ANYWHERE (~(phys_addr_t)0)
#define MEMBLOCK_ALLOC_ACCESSIBLE 0

/**
 * @brief 在指定物理地址范围内申请一段物理内存。
 * @param size 申请大小，单位为字节。
 * @param align 起始地址对齐要求。
 * @param start 搜索起始地址。
 * @param end 搜索结束地址；为 MEMBLOCK_ALLOC_ACCESSIBLE 时使用当前分配上限。
 * @return 成功返回分配得到的物理地址，失败返回 0。
 */
phys_addr_t ttos_memblk_phys_alloc_specify_range(phys_addr_t size, phys_addr_t align,
                                                 phys_addr_t start, phys_addr_t end);

/**
 * @brief 在全部可访问物理内存中申请一段按 @p align 对齐的物理页。
 * @param size 申请大小，单位为字节。
 * @param align 起始地址对齐要求。
 * @return 成功返回分配得到的物理地址，失败返回 0。
 */
static inline phys_addr_t ttos_memblk_phys_alloc(phys_addr_t size, phys_addr_t align)
{
    return ttos_memblk_phys_alloc_specify_range(size, align, 0, MEMBLOCK_ALLOC_ACCESSIBLE);
}

#endif /* _TTOS_MEMBLOCK_H */
