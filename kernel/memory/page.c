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
#include "memblock.h"
#include <assert.h>
#include <commonUtils.h>
#include <inttypes.h>
#include <mmu.h>
#include <spinlock.h>
#include <stdio.h>
#include <ttosMM.h>
#include <uaccess.h>

#define KLOG_LEVEL KLOG_WARN
#define KLOG_TAG "page"
#include <klog.h>

#define PAGE_LIST_SIZE (ARCH_ADDRESS_WIDTH_BITS - PAGE_SIZE_SHIFT)

static int page_nr;

extern struct page *vpage_start;

static struct page *page_list[PAGE_LIST_SIZE];

DEFINE_SPINLOCK(page_lock);

static bool page_is_zone(struct page *page, int zone);

struct page *pfn_to_page(pfn_t pfn)
{
    struct page *p = &vpage_start[pfn];
    /* 如果该地址无法访问则表示此处不存在物理内存 */
    if ((kernel_access_check(p, sizeof(struct page), UACCESS_R)))
    {
        return p;
    }
    return NULL;
}

pfn_t page_to_pfn(struct page *p)
{
    return p - vpage_start;
}

uint32_t page_bits(size_t size)
{
    int bit = sizeof(uintptr_t) * 8 - __builtin_clzl(size) - 1;

    if ((size ^ (1UL << bit)) != 0)
    {
        bit++;
    }
    bit -= PAGE_SIZE_SHIFT;
    if (bit < 0)
    {
        bit = 0;
    }
    return bit;
}

struct page *addr_to_page(phys_addr_t addr)
{
    return pfn_to_page(PFN_DOWN(addr));
}

phys_addr_t page_to_addr(struct page *p)
{
    return (phys_addr_t)page_to_pfn(p) << PAGE_SIZE_SHIFT;
}

struct page *alloc_page(void)
{
    phys_addr_t addr = pages_alloc(0, ZONE_NORMAL);
    if (addr)
    {
        return addr_to_page(addr);
    }
    return NULL;
}

void free_page(struct page *p)
{
    pages_free(page_to_addr(p), 0);
}

static inline struct page *buddy_get(struct page *p, uint32_t size_bits)
{
    phys_addr_t addr;

    addr = page_to_addr(p);
    addr ^= (1ULL << (size_bits + PAGE_SIZE_SHIFT));
    return addr_to_page(addr);
}

static void page_remove(struct page *p, uint32_t size_bits)
{
    if (p->pre)
    {
        p->pre->next = p->next;
    }
    else
    {
        page_list[size_bits] = p->next;
    }

    if (p->next)
    {
        p->next->pre = p->pre;
    }

    p->size_bits = ARCH_ADDRESS_WIDTH_BITS;
}

static void page_insert(struct page *p, uint32_t size_bits)
{
    p->next = page_list[size_bits];
    if (p->next)
    {
        p->next->pre = p;
    }
    p->pre = 0;
    page_list[size_bits] = p;
    p->size_bits = size_bits;
}

static void _pages_ref_inc(struct page *p, uint32_t size_bits)
{
    struct page *page_head;
    pfn_t pfn;

    /* find page group head */
    pfn = page_to_pfn(p);

    pfn = pfn & ~((1ULL << size_bits) - 1);

    page_head = pfn_to_page(pfn);
    if (page_head == NULL)
    {
        return;
    }
    page_head->ref_cnt++;
}

static int _pages_ref_get(struct page *p, uint32_t size_bits)
{
    struct page *page_head;
    pfn_t pfn;

    /* find page group head */
    pfn = page_to_pfn(p);

    pfn = pfn & ~((1ULL << size_bits) - 1);

    page_head = pfn_to_page(pfn);
    if (page_head == NULL)
    {
        return 0;
    }
    return page_head->ref_cnt;
}

static int _pages_free(struct page *p, uint32_t size_bits)
{
    uint32_t level = size_bits;
    struct page *buddy;
    pfn_t pfn;
    pfn = page_to_pfn(p);

    pfn = pfn & ~((1ULL << size_bits) - 1);

    p = pfn_to_page(pfn);
    if (p == NULL)
    {
        return 0;
    }

    assert(p->ref_cnt > 0);
    assert(p->size_bits == ARCH_ADDRESS_WIDTH_BITS);

    p->ref_cnt--;
    if (p->ref_cnt != 0)
    {
        return 0;
    }

    while (level < PAGE_LIST_SIZE)
    {
        buddy = buddy_get(p, level);
        if (buddy && buddy->size_bits == level &&
            (page_is_zone(buddy, ZONE_NORMAL) == page_is_zone(p, ZONE_NORMAL)))
        {
            page_remove(buddy, level);
            p = (p < buddy) ? p : buddy;
            level++;
        }
        else
        {
            break;
        }
    }
    page_insert(p, level);
    return 1;
}

static bool page_is_zone(struct page *page, int zone)
{
    return memory_is_zone(page_to_addr(page), zone);
}

static struct page *_pages_alloc(uint32_t size_bits, int zone)
{
    struct page *p;

    if (page_list[size_bits])
    {
        p = page_list[size_bits];
        while (p && !page_is_zone(p, zone))
        {
            p = p->next;
        }
        if (p)
        {
            page_remove(p, size_bits);
            p->size_bits = ARCH_ADDRESS_WIDTH_BITS;
            p->ref_cnt = 1;
            return p;
        }
    }

    {
        uint32_t level;
        p = NULL;
        for (level = size_bits + 1; level < PAGE_LIST_SIZE; level++)
        {
            if (page_list[level])
            {
                p = page_list[level];
                while (p && !page_is_zone(p, zone))
                {
                    p = p->next;
                }
                if (p)
                {
                    break;
                }
            }
        }
        if (level == PAGE_LIST_SIZE)
        {
            return 0;
        }
        if (p == NULL)
        {
            return 0;
        }
        page_remove(p, level);
        while (level > size_bits)
        {
            page_insert(p, level - 1);
            p = buddy_get(p, level - 1);
            level--;
        }
    }
    p->size_bits = ARCH_ADDRESS_WIDTH_BITS;
    p->ref_cnt = 1;
    return p;
}

int page_ref_get(phys_addr_t addr, uint32_t size_bits)
{
    struct page *p;
    uintptr_t flag;
    int ref;

    p = addr_to_page(addr);
    if (p == NULL)
    {
        return 0;
    }
    spin_lock_irqsave(&page_lock, flag);
    ref = _pages_ref_get(p, size_bits);
    spin_unlock_irqrestore(&page_lock, flag);
    return ref;
}

void page_ref_inc(phys_addr_t addr, uint32_t size_bits)
{
    struct page *p;
    uintptr_t flag;

    p = addr_to_page(addr);
    if (p == NULL)
    {
        return;
    }
    spin_lock_irqsave(&page_lock, flag);
    _pages_ref_inc(p, size_bits);
    spin_unlock_irqrestore(&page_lock, flag);
}

phys_addr_t pages_alloc(uint32_t size_bits, int zone)
{
    struct page *p;
    uintptr_t flag;

    spin_lock_irqsave(&page_lock, flag);
    p = _pages_alloc(size_bits, zone);
    spin_unlock_irqrestore(&page_lock, flag);

    if (p == NULL)
    {
        return 0;
    }
    return page_to_addr(p);
}

struct page *ptr_to_page(void *vaddr)
{
    return addr_to_page(mm_kernel_v2p((virt_addr_t)vaddr));
}

int pages_free(phys_addr_t addr, uint32_t size_bits)
{
    struct page *p;
    int real_free = 0;

    p = addr_to_page(addr);
    if (p)
    {
        uintptr_t flag;
        spin_lock_irqsave(&page_lock, flag);
        real_free = _pages_free(p, size_bits);
        spin_unlock_irqrestore(&page_lock, flag);
    }
    return real_free;
}

void list_page(void)
{
    int i;
    int total = 0;

    uintptr_t flag;
    spin_lock_irqsave(&page_lock, flag);

    for (i = 0; i < PAGE_LIST_SIZE; i++)
    {
        struct page *p = page_list[i];

        printk("level %d ", i);

        while (p)
        {
            total += (1ULL << i);
            printk("[%" PRIx64 "]", page_to_addr(p));
            p = p->next;
        }
        printk("\n");
    }
    spin_unlock_irqrestore(&page_lock, flag);
    printk("free pages is %d total is %d\n", total, page_nr);
    printk("-------------------------------\n");
}

void page_get_info(uintptr_t *total_nr, uintptr_t *free_nr)
{
    int i;
    uintptr_t total_free = 0;
    uintptr_t flag;

    spin_lock_irqsave(&page_lock, flag);
    for (i = 0; i < PAGE_LIST_SIZE; i++)
    {
        struct page *p = page_list[i];

        while (p)
        {
            total_free += (1UL << i);
            p = p->next;
        }
    }
    spin_unlock_irqrestore(&page_lock, flag);
    *total_nr = page_nr;
    *free_nr = total_free;
}

void set_page_free(phys_addr_t start, int order)
{
    struct page *p;
    uintptr_t flag;
    pfn_t start_pfn = PFN_DOWN(start);
    pfn_t end_pfn = start_pfn + (1 << order);

    if ((page_is_zone(pfn_to_page(start_pfn), ZONE_NORMAL) ==
         page_is_zone(pfn_to_page(end_pfn - 1), ZONE_NORMAL)))
    {
        p = pfn_to_page(start_pfn);
        if (!p)
        {
            KLOG_E("error: pfn_to_page : %" PRIx64 " struct page is %p\n",
                   (phys_addr_t)start_pfn << PAGE_SIZE_SHIFT, &vpage_start[start_pfn]);
            return;
        }
        p->size_bits = ARCH_ADDRESS_WIDTH_BITS;
        p->ref_cnt = 1;
        _pages_free(p, order);
    }
    else
    {
        set_page_free(start, order - 1);
        set_page_free(start + (1ULL << (order - 1 + PAGE_SIZE_SHIFT)), order - 1);
    }
}

void set_page_reserved(phys_addr_t start, phys_addr_t end)
{
    struct page *p;
    uintptr_t flag;
    pfn_t start_pfn = PFN_DOWN(start);
    pfn_t end_pfn = PFN_DOWN(end);
    pfn_t pfn;

    spin_lock_irqsave(&page_lock, flag);
    for (pfn = start_pfn; pfn < end_pfn; pfn++)
    {
        p = pfn_to_page(pfn);
        if (p)
        {
            p->size_bits = ARCH_ADDRESS_WIDTH_BITS;
            p->ref_cnt = 1;
        }
        else
        {
            KLOG_E("error: pfn_to_page : %" PRIx64 ", struct page is %p\n",
                   (phys_addr_t)pfn << PAGE_SIZE_SHIFT, &vpage_start[pfn]);
        }
    }
    spin_unlock_irqrestore(&page_lock, flag);
}

void totalram_pages_add(int count)
{
    page_nr += count;
}

virt_addr_t page_address(phys_addr_t addr)
{
    if (addr <= (max_low_pfn << PAGE_SIZE_SHIFT) && addr >= (min_low_pfn << PAGE_SIZE_SHIFT))
    {
        return (virt_addr_t)(addr - zone_normal_pvoffset());
    }
    else
    {
        return (virt_addr_t)0;
    }
}

#define MAX_REGIONS 256
#define MAX_TOTAL_REGIONS 16

// ------- 区域结构 -------
typedef struct mem_region
{
    uint64_t start;
    uint64_t size;
} mem_region_t;

// ------- 全局区域 -------
static mem_region_t free_regions[MAX_REGIONS];
static int free_region_count = 0;

static mem_region_t total_regions[MAX_TOTAL_REGIONS];
static int total_region_count = 0;

// ------- 辅助函数 -------
static void add_free_region(uint64_t addr, uint64_t size)
{
    if (free_region_count < MAX_REGIONS)
    {
        free_regions[free_region_count++] = (mem_region_t){addr, size};
    }
}

static int compare_regions(const void *a, const void *b)
{
    uint64_t sa = ((mem_region_t *)a)->start;
    uint64_t sb = ((mem_region_t *)b)->start;
    return (sa > sb) - (sa < sb);
}

static int merge_regions(mem_region_t *in, int count, mem_region_t *out)
{
    if (count == 0)
        return 0;
    qsort(in, count, sizeof(mem_region_t), compare_regions);
    int idx = 0;
    out[0] = in[0];
    for (int i = 1; i < count; i++)
    {
        uint64_t end = out[idx].start + out[idx].size;
        if (in[i].start <= end)
        {
            uint64_t new_end = in[i].start + in[i].size;
            if (new_end > end)
            {
                out[idx].size = new_end - out[idx].start;
            }
        }
        else
        {
            out[++idx] = in[i];
        }
    }
    return idx + 1;
}

static int subtract_regions(mem_region_t *total, int total_cnt, mem_region_t *free, int free_cnt,
                            mem_region_t *used)
{
    int used_cnt = 0;

    qsort(total, total_cnt, sizeof(mem_region_t), compare_regions);
    qsort(free, free_cnt, sizeof(mem_region_t), compare_regions);

    for (int i = 0; i < total_cnt; i++)
    {
        uint64_t ts = total[i].start;
        uint64_t te = total[i].start + total[i].size;

        for (int j = 0; j < free_cnt && ts < te; j++)
        {
            uint64_t fs = free[j].start;
            uint64_t fe = free[j].start + free[j].size;

            if (fe <= ts)
                continue;
            if (fs >= te)
                break;

            if (fs > ts)
            {
                used[used_cnt++] = (mem_region_t){ts, fs - ts};
            }

            ts = (fe > ts) ? fe : ts;
        }

        if (ts < te)
        {
            used[used_cnt++] = (mem_region_t){ts, te - ts};
        }
    }
    return used_cnt;
}

static void print_regions(const char *title, mem_region_t *r, int n)
{
    printf("== %s ==\n", title);
    for (int i = 0; i < n; i++)
    {
        printf("0x%llx - 0x%llx  (size: 0x%X)\n", r[i].start, r[i].start + r[i].size - 1,
               r[i].size);
    }
    printf("\n");
}

// ------- 模拟设置总地址空间 -------
void page_mem_regions_set(phys_addr_t addr, phys_addr_t size)
{
    total_regions[total_region_count] = (mem_region_t){addr, size};
    total_region_count++;
}

// #ifdef CONFIG_SHELL
#include <shell.h>

// ------- 原始命令 -------
static int phy_mem_cmd(size_t argc, char **argv)
{
    struct page *pg;
    for (int i = 0; i < PAGE_LIST_SIZE; i++)
    {
        pg = page_list[i];
        while (pg)
        {
            uint64_t addr = page_to_addr(pg);
            uint64_t size = 1ULL << (pg->size_bits + 12);
            add_free_region(addr, size);
            pg = pg->next;
        }
    }

    // 合并空闲区
    mem_region_t merged[MAX_REGIONS];
    int merged_cnt = merge_regions(free_regions, free_region_count, merged);

    // 计算 used 区域
    mem_region_t used[MAX_REGIONS];
    int used_cnt = subtract_regions(total_regions, total_region_count, merged, merged_cnt, used);

    // 打印结果
    print_regions("Free", merged, merged_cnt);
    print_regions("Used", used, used_cnt);
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) |
                     SHELL_CMD_DISABLE_RETURN,
                 phymem, phy_mem_cmd, show phy mem);

static int _page_cmd(int argc, const char *argv[])
{
    list_page();
    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) |
                     SHELL_CMD_DISABLE_RETURN,
                 page, _page_cmd, list page info);
// #endif