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
 * @file memblock.c
 * @brief 启动早期的物理内存区间管理与页级分配实现。
 *
 * 这份 memblock 实现只保留启动阶段真正需要的最小能力：
 * 1. 维护 memory / reserved 两组物理区间
 * 2. 在可用区间中做简单的页级分配
 * 3. 在页分配器初始化时，把剩余物理页交给伙伴系统
 */
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <linux/bug.h>
#include <memblock.h>
#include <mmu.h>
#include <page.h>
#include <string.h>
#include <system/compiler.h>
#include <system/macros.h>
#include <ttosMM.h>
#include <uaccess.h>
#include <util.h>

#define KLOG_TAG "Memblock"
#include <klog.h>

#undef KLOG_D
#define KLOG_D(...)

#define MAX_ORDER 64
#define L1_CACHE_BYTES 32

#ifndef SMP_CACHE_BYTES
#define SMP_CACHE_BYTES L1_CACHE_BYTES
#endif

#define DEFAULT_MEMBLOCK_REGIONS 128
#define DEFAULT_MEMBLOCK_RESERVED_REGIONS 128
/** @brief 一次区间扫描最多输出的切分结果数。 */
#define MEMBLOCK_MAX_ITER_RANGES (DEFAULT_MEMBLOCK_REGIONS + DEFAULT_MEMBLOCK_RESERVED_REGIONS + 4)

/** @brief 固定容量的启动期可用区表。 */
static struct ttos_memblk_region static_ttos_memblk_memory_regions[DEFAULT_MEMBLOCK_REGIONS];
/** @brief 固定容量的启动期保留区表。 */
static struct ttos_memblk_region
    static_ttos_memblk_reserved_regions[DEFAULT_MEMBLOCK_RESERVED_REGIONS];

struct ttos_memblk memblock = {
    .memory.regions = static_ttos_memblk_memory_regions,
    .memory.rgn_num = 0,
    .memory.rgn_max = DEFAULT_MEMBLOCK_REGIONS,
    .memory.name = "memory",
    .reserved.regions = static_ttos_memblk_reserved_regions,
    .reserved.rgn_num = 0,
    .reserved.rgn_max = DEFAULT_MEMBLOCK_RESERVED_REGIONS,
    .reserved.name = "reserved",
    .bottom_up = false,
    .current_limit = MEMBLOCK_ALLOC_ANYWHERE,
};

pfn_t max_low_pfn;
pfn_t min_low_pfn = UINTPTR_MAX;
struct page *vpage_start;
pfn_t max_pfn;
pfn_t max_possible_pfn = PHYS_ADDR_MAX >> PAGE_SIZE_SHIFT;

/**
 * @brief 将物理地址转换为线性映射下的内核虚拟地址。
 * @param x 物理地址。
 * @return 对应的内核虚拟地址。
 */
static inline void *phys_to_virt(phys_addr_t x)
{
    return (void *)(virt_addr_t)(x - zone_normal_pvoffset());
}

/**
 * @brief 将线性映射下的内核虚拟地址转换为物理地址。
 * @param x 内核虚拟地址。
 * @return 对应的物理地址。
 */
static inline phys_addr_t virt_to_phys(virt_addr_t x)
{
    return x + zone_normal_pvoffset();
}

/**
 * @brief 选择本次分配使用的区间属性过滤条件。
 * @return 当前始终返回 MEMBLOCK_NONE。
 */
static inline enum ttos_memblk_flags ttos_memblk_choose_alloc_flags(void)
{
    return MEMBLOCK_NONE;
}

/**
 * @brief 判断区间是否带有 NOMAP 属性。
 * @param region 待检查区间。
 * @return true 表示该区间不应参与默认映射，false 表示不是。
 */
static inline bool ttos_memblk_region_is_nomap(const struct ttos_memblk_region *region)
{
    return (region->flags & MEMBLOCK_NOMAP) != 0;
}

/**
 * @brief 将输入区间规整为半开区间 [start, end)。
 * @param start 区间起始地址。
 * @param size 区间大小。
 * @param end_out 输出规整后的结束地址。
 * @return true 表示得到有效区间，false 表示输入无效。
 */
static inline bool ttos_memblk_normalize_range(phys_addr_t start, phys_addr_t size,
                                               phys_addr_t *end_out)
{
    phys_addr_t end;

    if (!size)
        return false;

    if (size > PHYS_ADDR_MAX - start)
        end = PHYS_ADDR_MAX;
    else
        end = start + size;

    if (end <= start)
        return false;

    *end_out = end;
    return true;
}

/**
 * @brief 判断两个半开区间是否相交。
 */
static inline bool ttos_memblk_region_overlap(phys_addr_t start_a, phys_addr_t end_a,
                                              phys_addr_t start_b, phys_addr_t end_b)
{
    return start_a < end_b && start_b < end_a;
}

/**
 * @brief 重新统计区间集合的总大小。
 * @param collect 待统计集合。
 */
static void ttos_memblk_recount_total(struct ttos_memblk_collect *collect)
{
    phys_addr_t total = 0;
    unsigned long i;

    for (i = 0; i < collect->rgn_num; i++)
        total += collect->regions[i].size;

    collect->total_size = total;
}

/**
 * @brief 在集合中插入一个新区间。
 * @param collect 目标集合。
 * @param idx 插入位置。
 * @param start 区间起始地址。
 * @param size 区间大小。
 * @param flags 区间属性。
 * @return 成功返回 0，失败返回负错误码。
 */
static int ttos_memblk_insert_slot(struct ttos_memblk_collect *collect, unsigned long idx,
                                   phys_addr_t start, phys_addr_t size,
                                   enum ttos_memblk_flags flags)
{
    struct ttos_memblk_region *region;

    if (collect->rgn_num >= collect->rgn_max)
        return -ENOMEM;

    if (idx < collect->rgn_num)
    {
        memmove(&collect->regions[idx + 1], &collect->regions[idx],
                (collect->rgn_num - idx) * sizeof(*collect->regions));
    }

    region = &collect->regions[idx];
    region->start = start;
    region->size = size;
    region->flags = flags;
    collect->rgn_num++;
    collect->total_size += size;
    return 0;
}

/**
 * @brief 从集合中删除一个区间槽位。
 * @param collect 目标集合。
 * @param idx 待删除槽位。
 */
static void ttos_memblk_remove_slot(struct ttos_memblk_collect *collect, unsigned long idx)
{
    collect->total_size -= collect->regions[idx].size;
    if (idx + 1 < collect->rgn_num)
    {
        memmove(&collect->regions[idx], &collect->regions[idx + 1],
                (collect->rgn_num - idx - 1) * sizeof(*collect->regions));
    }
    collect->rgn_num--;
}

/**
 * @brief 合并相邻或重叠且属性相同的区间。
 * @param collect 目标集合。
 */
static void ttos_memblk_merge_regions(struct ttos_memblk_collect *collect)
{
    unsigned long i = 0;

    while (i + 1 < collect->rgn_num)
    {
        struct ttos_memblk_region *curr = &collect->regions[i];
        struct ttos_memblk_region *next = &collect->regions[i + 1];
        phys_addr_t curr_end = curr->start + curr->size;
        phys_addr_t next_end = next->start + next->size;

        if (curr->flags != next->flags || curr_end < next->start)
        {
            i++;
            continue;
        }

        if (next_end > curr_end)
            curr->size = next_end - curr->start;

        ttos_memblk_remove_slot(collect, i + 1);
        ttos_memblk_recount_total(collect);
    }
}

/**
 * @brief 将一段区间加入指定集合。
 *
 * 已存在的重叠部分保持不变，只补入原集合尚未覆盖的部分。
 *
 * @param collect 目标集合。
 * @param start 区间起始地址。
 * @param size 区间大小。
 * @param flags 区间属性。
 * @return 成功返回 0，失败返回负错误码。
 */
static int ttos_memblk_add_range(struct ttos_memblk_collect *collect, phys_addr_t start,
                                 phys_addr_t size, enum ttos_memblk_flags flags)
{
    phys_addr_t end;
    phys_addr_t cursor;
    unsigned long idx = 0;
    int ret;

    if (!ttos_memblk_normalize_range(start, size, &end))
        return -EINVAL;

    while (idx < collect->rgn_num &&
           collect->regions[idx].start + collect->regions[idx].size <= start)
        idx++;

    cursor = start;

    while (idx < collect->rgn_num && collect->regions[idx].start < end)
    {
        phys_addr_t region_start = collect->regions[idx].start;
        phys_addr_t region_end = region_start + collect->regions[idx].size;

        if (cursor < region_start)
        {
            ret = ttos_memblk_insert_slot(collect, idx, cursor, region_start - cursor, flags);
            if (ret)
                return ret;
            idx++;
        }

        if (region_end > cursor)
            cursor = region_end;

        if (cursor >= end)
            break;

        idx++;
    }

    if (cursor < end)
    {
        ret = ttos_memblk_insert_slot(collect, idx, cursor, end - cursor, flags);
        if (ret)
            return ret;
    }

    ttos_memblk_merge_regions(collect);
    return 0;
}

/**
 * @brief 从指定集合中移除一段区间。
 *
 * 如有必要，会把一个区间拆成前后两段保留下来。
 *
 * @param collect 目标集合。
 * @param start 区间起始地址。
 * @param size 区间大小。
 * @return 成功返回 0，失败返回负错误码。
 */
static int ttos_memblk_remove_range(struct ttos_memblk_collect *collect, phys_addr_t start,
                                    phys_addr_t size)
{
    phys_addr_t end;
    unsigned long idx = 0;

    if (!ttos_memblk_normalize_range(start, size, &end))
        return -EINVAL;

    while (idx < collect->rgn_num)
    {
        struct ttos_memblk_region *region = &collect->regions[idx];
        phys_addr_t region_start = region->start;
        phys_addr_t region_end = region_start + region->size;

        if (region_start >= end)
            break;

        if (!ttos_memblk_region_overlap(region_start, region_end, start, end))
        {
            idx++;
            continue;
        }

        if (start <= region_start && end >= region_end)
        {
            ttos_memblk_remove_slot(collect, idx);
            continue;
        }

        if (start <= region_start)
        {
            collect->total_size -= end - region_start;
            region->start = end;
            region->size = region_end - end;
            idx++;
            continue;
        }

        if (end >= region_end)
        {
            collect->total_size -= region_end - start;
            region->size = start - region_start;
            idx++;
            continue;
        }

        if (collect->rgn_num >= collect->rgn_max)
            return -ENOMEM;

        memmove(&collect->regions[idx + 2], &collect->regions[idx + 1],
                (collect->rgn_num - idx - 1) * sizeof(*collect->regions));
        collect->regions[idx + 1].start = end;
        collect->regions[idx + 1].size = region_end - end;
        collect->regions[idx + 1].flags = region->flags;
        collect->rgn_num++;
        collect->total_size -= end - start;
        region->size = start - region_start;
        idx += 2;
    }

    return 0;
}

/**
 * @brief 判断给定区间是否与集合中的任一区间相交。
 * @param collect 目标集合。
 * @param start 区间起始地址。
 * @param size 区间大小。
 * @return true 表示存在相交，false 表示不存在。
 */
static int ttos_memblk_range_intersect_collect(struct ttos_memblk_collect *collect,
                                               phys_addr_t start, phys_addr_t size)
{
    phys_addr_t end;
    unsigned long i;

    if (!ttos_memblk_normalize_range(start, size, &end))
        return false;

    for (i = 0; i < collect->rgn_num; i++)
    {
        phys_addr_t region_start = collect->regions[i].start;
        phys_addr_t region_end = region_start + collect->regions[i].size;

        if (region_start >= end)
            break;

        if (ttos_memblk_region_overlap(region_start, region_end, start, end))
            return true;
    }

    return false;
}

/**
 * @brief 添加可用物理内存区间。
 *
 * 同时刷新普通内存区相关的 PFN 边界，这些边界后续会被页描述符映射和伙伴系统初始化使用。
 *
 * @param start 区间起始地址。
 * @param size 区间大小。
 * @return 成功返回 0，失败返回负错误码。
 */
int ttos_memblk_add(phys_addr_t start, phys_addr_t size)
{
    phys_addr_t end;
    phys_addr_t zone_normal_start;
    phys_addr_t zone_normal_end;

    if (!ttos_memblk_normalize_range(start, size, &end))
        return -EINVAL;

    max_pfn = max(max_pfn, PFN_DOWN(end - 1));

    zone_normal_start = virt_to_phys(ZONE_NORMAL_START);
    zone_normal_end = virt_to_phys(ZONE_NORMAL_END);

    min_low_pfn = max(min(min_low_pfn, PFN_DOWN(start)), PFN_DOWN(zone_normal_start));
    max_low_pfn = min(max(max_low_pfn, PFN_DOWN(end - 1)), PFN_DOWN(zone_normal_end));

    return ttos_memblk_add_range(&memblock.memory, start, size, MEMBLOCK_NONE);
}

/**
 * @brief 添加一段保留区。
 * @param start 区间起始地址。
 * @param size 区间大小。
 * @return 成功返回 0，失败返回负错误码。
 */
int ttos_memblk_reserve(phys_addr_t start, phys_addr_t size)
{
    return ttos_memblk_add_range(&memblock.reserved, start, size, MEMBLOCK_NONE);
}

/**
 * @brief 从可用区中删除一段区间。
 * @param start 区间起始地址。
 * @param size 区间大小。
 * @return 成功返回 0，失败返回负错误码。
 */
int ttos_memblk_remove(phys_addr_t start, phys_addr_t size)
{
    return ttos_memblk_remove_range(&memblock.memory, start, size);
}

/**
 * @brief 从保留区中删除一段区间。
 * @param start 区间起始地址。
 * @param size 区间大小。
 * @return 成功返回 0，失败返回负错误码。
 */
int ttos_memblk_free(phys_addr_t start, phys_addr_t size)
{
    return ttos_memblk_remove_range(&memblock.reserved, start, size);
}

/**
 * @brief 判断遍历时是否需要跳过某个区间。
 * @return true 表示跳过，false 表示保留。
 */
static bool ttos_memblk_skip_region(struct ttos_memblk_collect *collect,
                                    struct ttos_memblk_region *region, enum ttos_memblk_flags flags)
{
    if (collect != &memblock.memory)
        return false;

    if (!(flags & MEMBLOCK_NOMAP) && ttos_memblk_region_is_nomap(region))
        return true;

    return false;
}

/**
 * @brief 收集指定集合中的有效区间。
 *
 * 如果提供 collect_b，则输出 collect_a - collect_b 的差集结果。
 *
 * @param collect_a 被遍历集合。
 * @param collect_b 需要排除的集合，可为 NULL。
 * @param flags 区间属性过滤条件。
 * @param out 输出数组。
 * @param out_max 输出数组容量。
 * @return 实际输出的区间数。
 */
static unsigned long ttos_memblk_collect_ranges(struct ttos_memblk_collect *collect_a,
                                                struct ttos_memblk_collect *collect_b,
                                                enum ttos_memblk_flags flags,
                                                struct ttos_memblk_region *out,
                                                unsigned long out_max)
{
    unsigned long count = 0;
    unsigned long i;

    for (i = 0; i < collect_a->rgn_num; i++)
    {
        struct ttos_memblk_region *a = &collect_a->regions[i];
        phys_addr_t cursor;
        phys_addr_t a_end;
        unsigned long j;

        if (ttos_memblk_skip_region(collect_a, a, flags))
            continue;

        if (count >= out_max)
            break;

        if (!collect_b)
        {
            out[count++] = *a;
            continue;
        }

        cursor = a->start;
        a_end = a->start + a->size;

        for (j = 0; j < collect_b->rgn_num && cursor < a_end; j++)
        {
            phys_addr_t b_start = collect_b->regions[j].start;
            phys_addr_t b_end = b_start + collect_b->regions[j].size;

            if (b_end <= cursor)
                continue;
            if (b_start >= a_end)
                break;

            if (cursor < b_start)
            {
                if (count >= out_max)
                    return count;
                out[count].start = cursor;
                out[count].size = b_start - cursor;
                out[count].flags = a->flags;
                count++;
            }

            if (b_end > cursor)
                cursor = min(b_end, a_end);
        }

        if (cursor < a_end)
        {
            if (count >= out_max)
                break;
            out[count].start = cursor;
            out[count].size = a_end - cursor;
            out[count].flags = a->flags;
            count++;
        }
    }

    return count;
}

/**
 * @brief 正向返回一段有效区间，供内部遍历使用。
 */
static void __next_range(u64 *idx, enum ttos_memblk_flags flags,
                         struct ttos_memblk_collect *collect_a,
                         struct ttos_memblk_collect *collect_b, phys_addr_t *out_start,
                         phys_addr_t *out_end)
{
    struct ttos_memblk_region ranges[MEMBLOCK_MAX_ITER_RANGES];
    unsigned long count;
    unsigned long pos;

    count = ttos_memblk_collect_ranges(collect_a, collect_b, flags, ranges, ARRAY_SIZE(ranges));
    pos = (*idx == ULLONG_MAX) ? count : (unsigned long)*idx;

    if (pos >= count)
    {
        *idx = ULLONG_MAX;
        return;
    }

    if (out_start)
        *out_start = ranges[pos].start;
    if (out_end)
        *out_end = ranges[pos].start + ranges[pos].size;

    pos++;
    *idx = (pos >= count) ? ULLONG_MAX : pos;
}

/**
 * @brief 反向返回一段有效区间，供内部遍历使用。
 */
static void __next_range_reverse(u64 *idx, enum ttos_memblk_flags flags,
                                 struct ttos_memblk_collect *collect_a,
                                 struct ttos_memblk_collect *collect_b, phys_addr_t *out_start,
                                 phys_addr_t *out_end)
{
    struct ttos_memblk_region ranges[MEMBLOCK_MAX_ITER_RANGES];
    unsigned long count;
    unsigned long seen;
    unsigned long pos;

    count = ttos_memblk_collect_ranges(collect_a, collect_b, flags, ranges, ARRAY_SIZE(ranges));
    seen = (*idx == ULLONG_MAX) ? 0 : (unsigned long)*idx;

    if (seen >= count)
    {
        *idx = ULLONG_MAX;
        return;
    }

    pos = count - seen - 1;
    if (out_start)
        *out_start = ranges[pos].start;
    if (out_end)
        *out_end = ranges[pos].start + ranges[pos].size;

    seen++;
    *idx = (seen >= count) ? ULLONG_MAX : seen;
}

/**
 * @brief 统一处理分配搜索范围。
 * @return true 表示范围有效，false 表示无有效范围。
 */
static phys_addr_t ttos_memblk_modify_alloc_range(phys_addr_t *range_start, phys_addr_t *range_end)
{
    if (*range_end == MEMBLOCK_ALLOC_ACCESSIBLE)
        *range_end = memblock.current_limit;

    *range_start = max(*range_start, (phys_addr_t)PAGE_SIZE);

    return *range_start < *range_end;
}

/**
 * @brief 在指定物理地址范围内查找一段未保留区间。
 *
 * 默认优先自顶向下搜索，以便把低地址尽量留给后续固定映射和早期结构。
 *
 * @param size 申请大小。
 * @param align 对齐要求。
 * @param range_start 搜索起始地址。
 * @param range_end 搜索结束地址。
 * @param flags 区间属性过滤条件。
 * @return 成功返回物理地址，失败返回 0。
 */
static phys_addr_t ttos_memblk_request_size_in_range(phys_addr_t size, phys_addr_t align,
                                                     phys_addr_t range_start, phys_addr_t range_end,
                                                     enum ttos_memblk_flags flags)
{
    struct ttos_memblk_region ranges[MEMBLOCK_MAX_ITER_RANGES];
    unsigned long count;
    unsigned long i;

    if (!size)
        return 0;

    if (!align)
        align = SMP_CACHE_BYTES;

    if (!ttos_memblk_modify_alloc_range(&range_start, &range_end))
        return 0;

    count = ttos_memblk_collect_ranges(&memblock.memory, &memblock.reserved, flags, ranges,
                                       ARRAY_SIZE(ranges));

    if (!memblock.bottom_up)
    {
        for (i = count; i > 0; i--)
        {
            phys_addr_t start = clamp(ranges[i - 1].start, range_start, range_end);
            phys_addr_t end =
                clamp(ranges[i - 1].start + ranges[i - 1].size, range_start, range_end);
            phys_addr_t dest;

            if (end <= start || end - start < size)
                continue;

            dest = round_down(end - size, align);
            if (dest >= start)
                return dest;
        }

        return 0;
    }

    for (i = 0; i < count; i++)
    {
        phys_addr_t start = clamp(ranges[i].start, range_start, range_end);
        phys_addr_t end = clamp(ranges[i].start + ranges[i].size, range_start, range_end);
        phys_addr_t dest;

        if (end <= start)
            continue;

        dest = round_up(start, align);
        if (dest < end && end - dest >= size)
            return dest;
    }

    return 0;
}

/**
 * @brief 在指定范围内分配一段物理内存并加入 reserved。
 * @return 成功返回物理地址，失败返回 0。
 */
static phys_addr_t ttos_memblk_alloc_in_specify_range(phys_addr_t size, phys_addr_t align,
                                                      phys_addr_t range_start,
                                                      phys_addr_t range_end)
{
    enum ttos_memblk_flags flags = ttos_memblk_choose_alloc_flags();
    phys_addr_t result;

    if (!align)
        align = SMP_CACHE_BYTES;

    result = ttos_memblk_request_size_in_range(size, align, range_start, range_end, flags);
    if (!result)
        return 0;

    if (ttos_memblk_reserve(result, size))
        return 0;

    return result;
}

/**
 * @brief 对外提供的启动期物理页分配接口。
 * @return 成功返回物理地址，失败返回 0。
 */
phys_addr_t ttos_memblk_phys_alloc_specify_range(phys_addr_t size, phys_addr_t align,
                                                 phys_addr_t min_addr, phys_addr_t max_addr)
{
    return ttos_memblk_alloc_in_specify_range(size, align, min_addr, max_addr);
}

/**
 * @brief 在有序区间表中查找一个物理地址所属的区间。
 * @return 返回区间下标，未找到返回 -1。
 */
static int ttos_memblk_collect_search_addr(struct ttos_memblk_collect *collect, phys_addr_t addr)
{
    unsigned long left = 0;
    unsigned long right = collect->rgn_num;

    while (left < right)
    {
        unsigned long mid = left + (right - left) / 2;
        phys_addr_t start = collect->regions[mid].start;
        phys_addr_t end = start + collect->regions[mid].size;

        if (addr < start)
            right = mid;
        else if (addr >= end)
            left = mid + 1;
        else
            return mid;
    }

    return -1;
}

/**
 * @brief 调试输出指定区间集合。
 */
static void ttos_memblk_dump(struct ttos_memblk_collect *collect)
{
    unsigned long i;

    KLOG_D("%s:", collect->name);
    for (i = 0; i < collect->rgn_num; i++)
    {
        phys_addr_t start = collect->regions[i].start;
        phys_addr_t end = start + collect->regions[i].size - 1;

        KLOG_D("    [%" PRIx64 ", %" PRIx64 "]", start, end);
    }
}

/**
 * @brief 调试输出全部 memblock 状态。
 */
static void ttos_memblk_dump_all(void)
{
    KLOG_D("memblock configuration:");
    KLOG_D("memory size = 0x%" PRIx64 " reserved size = 0x%" PRIx64, memblock.memory.total_size,
           memblock.reserved.total_size);
    ttos_memblk_dump(&memblock.memory);
    ttos_memblk_dump(&memblock.reserved);
}

/**
 * @brief 简单自测入口，主要用于观察 add / alloc / free 的行为。
 * @return 固定返回 0。
 */
int memblock_test(void)
{
    phys_addr_t ptr1, ptr2;
    virt_addr_t vptr1;

    printk("********after add\n");
    ttos_memblk_dump_all();
    printk("\n\n\n");

    ptr1 = ttos_memblk_phys_alloc(0x8000, 0x1000);
    vptr1 = fix_map_set(1, ptr1, MT_KERNEL_MEM);
    printk("test vptr1 %p, ret %d\n", (void *)vptr1,
           kernel_access_check((void *)vptr1, 4, UACCESS_R));
    ptr2 = ttos_memblk_phys_alloc(0x80000, 0x1000);
    printk("********after alloc ,ptr1 0x%" PRIx64 " ptr2 0x%" PRIx64 " \n", ptr1, ptr2);
    ttos_memblk_dump_all();
    printk("\n\n\n");

    ttos_memblk_free(ptr1, 0x8000);
    ttos_memblk_free(ptr2, 0x80000);
    printk("********after free\n");
    ttos_memblk_dump_all();
    printk("\n\n\n");
    return 0;
}

void memblk_dump_all(void)
{
    ttos_memblk_dump_all();
}

/**
 * @brief 对一段可用页做批量释放，交给伙伴系统管理。
 * @return 释放的页数。
 */
static pfn_t __ttos_memblk_release_avail_mem_core(phys_addr_t start, phys_addr_t end)
{
    pfn_t start_pfn = PFN_UP(start);
    pfn_t end_pfn = PFN_UP(end);
    pfn_t temp_start;
    pfn_t temp_end;

    if (start_pfn >= end_pfn)
        return 0;

    temp_start = max(start_pfn, min_low_pfn);
    temp_end = end_pfn;

    /* 尽可能按大块释放到伙伴系统，减少启动阶段逐页释放的开销。 */
    while (temp_start < temp_end)
    {
        int order;
        int align_order;

        order = min(MAX_ORDER - 1ULL, (pfn_t)__builtin_ffsll(temp_start));
        while (temp_start + (1ULL << order) > temp_end)
            order--;

        align_order = __builtin_ctzl(temp_start);
        if (align_order < order)
            order = align_order;

        set_page_free(temp_start << PAGE_SIZE_SHIFT, order);
        temp_start += 1ULL << order;
    }

    return end_pfn - start_pfn;
}

/**
 * @brief 为 [start, end) 对应的 struct page 数组分配并建立映射。
 * @return 该物理区间包含的页数。
 */
static pfn_t setup_page_area(phys_addr_t start, phys_addr_t end)
{
    pfn_t start_pfn = PFN_DOWN(start);
    pfn_t end_pfn = PFN_UP(end);
    virt_addr_t vaddr;
    phys_addr_t paddr;
    size_t page_cnt;

    if (start_pfn >= end_pfn)
        return 0;

    vaddr = (virt_addr_t)&vpage_start[start_pfn];
    vaddr = PAGE_ALIGN_DOWN(vaddr);
    page_cnt = DIV_ROUND_UP(end_pfn - start_pfn, PAGE_SIZE / sizeof(struct page));

    paddr = ttos_memblk_phys_alloc(page_cnt * PAGE_SIZE, PAGE_SIZE);
    if (!paddr)
        return 0;

    kmap(vaddr, paddr, page_cnt * PAGE_SIZE, MT_KERNEL_MEM);
    return end_pfn - start_pfn;
}

/**
 * @brief 将 memblock 状态转换成页分配器可直接接管的形式。
 *
 * 1. 为每段物理内存建立 struct page 描述符映射
 * 2. 标记 reserved 区间
 * 3. 把未保留的页释放给伙伴系统
 *
 * @return 当前实现固定返回 0。
 */
unsigned long ttos_memblk_release_mem_to_pagealloc(void)
{
    struct ttos_memblk_region ranges[MEMBLOCK_MAX_ITER_RANGES];
    unsigned long count = 0;
    unsigned long i;
    unsigned long range_count;

    vpage_start = (struct page *)phys_to_virt(max_low_pfn << PAGE_SIZE_SHIFT);

    range_count = ttos_memblk_collect_ranges(&memblock.memory, NULL, MEMBLOCK_NONE, ranges,
                                             ARRAY_SIZE(ranges));
    for (i = 0; i < range_count; i++)
        count += setup_page_area(ranges[i].start, ranges[i].start + ranges[i].size);

    range_count = ttos_memblk_collect_ranges(&memblock.reserved, NULL, MEMBLOCK_NONE, ranges,
                                             ARRAY_SIZE(ranges));
    for (i = 0; i < range_count; i++)
    {
        phys_addr_t start = ranges[i].start;
        phys_addr_t end = start + ranges[i].size;

        set_page_reserved(start, end);
    }

    range_count = ttos_memblk_collect_ranges(&memblock.memory, &memblock.reserved, MEMBLOCK_NONE,
                                             ranges, ARRAY_SIZE(ranges));
    for (i = 0; i < range_count; i++)
        __ttos_memblk_release_avail_mem_core(ranges[i].start, ranges[i].start + ranges[i].size);

    totalram_pages_add(count);
    return 0;
}
