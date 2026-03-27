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

#include <cache.h>
#include <commonUtils.h>
#include <arch_helpers.h>
#include <cpu.h>
#include <inttypes.h>
#include <memblock.h>
#include <mmu.h>
#include <page.h>
#include <spinlock.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <system/kconfig.h>
#include <ttosMM.h>
#include <uaccess.h>

#define KLOG_TAG "MM"
#include <klog.h>

#define V2P_BY_REG 0

static struct mm *gp_kernel_mm;

uint64_t __attribute__((aligned(PAGE_SIZE))) _kernel_mmu_table0[512];

#ifdef __aarch64__
uint64_t __attribute__((aligned(PAGE_SIZE))) g_fix_map_level1[512];
uint64_t __attribute__((aligned(PAGE_SIZE))) g_fix_map_level2[512];
#endif

uint64_t __attribute__((aligned(PAGE_SIZE))) g_fix_map_level3[512];

extern int __executable_start;

#define VA_RAM_BASE ((virt_addr_t)KERNEL_SPACE_START)
#define VA_RAM_END (VA_RAM_BASE + ((max_low_pfn - min_low_pfn + 1) << PAGE_SIZE_SHIFT))

static DEFINE_SPINLOCK(ptable_fixmap_lock);
static DEFINE_SPINLOCK(fixmap_lock);
static DEFINE_SPINLOCK(ktmp_access_lock);

static void mmu_table_flush(virt_addr_t vaddr);
virt_addr_t arm_alloc_vaddr(struct mm *mm, size_t page_count);
static virt_addr_t arm_alloc_vaddr_with_min(struct mm *mm, size_t page_count, virt_addr_t map_min);
static int mmu_map_r(struct mm *mm, virt_addr_t va, phys_addr_t pa, size_t size, uint64_t attr);
static int arm_mmu_unmap(struct mm *mm, struct mm_region *region);
static uint64_t attr_2_mmu(uint64_t attr);

static fixmap_set_t fixmap_pool;

int g_last_fixmap = FIX_MAP_POOL_START;

static int fixmap_alloc(void)
{
    int fixmap;
    long flags;

    spin_lock_irqsave(&fixmap_lock, flags);
    while (FIXMAP_ISSET(g_last_fixmap, &fixmap_pool))
    {
        g_last_fixmap++;
        if (g_last_fixmap == FIXMAP_SETSIZE)
        {
            g_last_fixmap = FIX_MAP_POOL_START;
        }
    };
#ifdef FIX_MAP_USER_HLEPER
    if (unlikely(g_last_fixmap == FIX_MAP_USER_HLEPER))
    {
        FIXMAP_SET(g_last_fixmap, &fixmap_pool);
        g_last_fixmap++;
    }
#endif
    FIXMAP_SET(g_last_fixmap, &fixmap_pool);
    fixmap = g_last_fixmap++;
    if (g_last_fixmap == FIXMAP_SETSIZE)
    {
        g_last_fixmap = FIX_MAP_POOL_START;
    }
    spin_unlock_irqrestore(&fixmap_lock, flags);
    return fixmap;
}

static void fixmap_free(int fixmap)
{
    long flags;

    spin_lock_irqsave(&fixmap_lock, flags);
    FIXMAP_CLR(fixmap, &fixmap_pool);
    spin_unlock_irqrestore(&fixmap_lock, flags);
}

virt_addr_t fix_map_vaddr(int idx)
{
    return FIX_MAP_START + (idx << PAGE_SIZE_SHIFT);
}

static int fix_map_index(virt_addr_t vaddr)
{
    return (vaddr - FIX_MAP_START) >> PAGE_SIZE_SHIFT;
}

virt_addr_t ktmp_access_start(phys_addr_t paddr)
{
    virt_addr_t vaddr;
    vaddr = page_address(paddr);
    if (vaddr)
    {
        return vaddr;
    }
    int fixmap = fixmap_alloc();
    return fix_map_set(fixmap, paddr, MT_KERNEL_MEM);
}

void ktmp_access_end(virt_addr_t vaddr)
{
    if (vaddr >= fix_map_vaddr(FIX_MAP_POOL_START))
    {
        int idx = fix_map_index(vaddr);
        fixmap_free(idx);
        fix_map_set(idx, 0, 0);
    }
}

virt_addr_t fix_map_set(int idx, phys_addr_t phy, uint64_t prot)
{
    virt_addr_t vaddr;
    /* 不可以修改自映射 */
    if (FIX_MAP_SELF == idx)
    {
        return 0;
    }
    uint64_t *fix_table = (uint64_t *)fix_map_vaddr(FIX_MAP_SELF);
    uint64_t attr = attr_2_mmu(prot);
    vaddr = fix_map_vaddr(idx);
    if (phy == 0)
    {
        if ((fix_table[idx] != 0) && kernel_access_check((void *)vaddr, PAGE_SIZE, UACCESS_R))
        {
            cache_dcache_clean(vaddr, PAGE_SIZE);
        }

        fix_table[idx] = 0;
        cache_dcache_clean((virt_addr_t)&fix_table[idx], sizeof(uint64_t));
        mmu_table_flush(vaddr);
    }
    else
    {
        if (fix_table[idx] == PAGE_DESC(attr, phy))
        {
            return vaddr;
        }
        if ((fix_table[idx] != 0) && kernel_access_check((void *)vaddr, PAGE_SIZE, UACCESS_R))
        {
            cache_dcache_clean(vaddr, PAGE_SIZE);
        }
        fix_table[idx] = PAGE_DESC(attr, phy);
        cache_dcache_clean((virt_addr_t)&fix_table[idx], sizeof(uint64_t));
        mmu_table_flush(vaddr);
        cache_dcache_invalidate(vaddr, PAGE_SIZE);
    }
    return vaddr;
}

static phys_addr_t ptable_alloc(void)
{
    if (page_allocer_inited())
    {
        return pages_alloc(0, ZONE_ANYWHERE);
    }
    return ttos_memblk_phys_alloc(PAGE_SIZE, PAGE_SIZE);
}

static void ptable_free(phys_addr_t phy)
{
    if (page_allocer_inited())
    {
        pages_free(phy, 0);
        return;
    }
    ttos_memblk_free(phy, PAGE_SIZE);
}

static uint64_t *ptable_get(phys_addr_t pte, int level, bool *use_fixmap)
{
    if (page_allocer_inited())
    {
        virt_addr_t vaddr = page_address(pte);
        if (vaddr)
        {
            *use_fixmap = false;
            return (uint64_t *)vaddr;
        }
    }

    spin_lock(&ptable_fixmap_lock);
    *use_fixmap = true;
    return (uint64_t *)fix_map_set(FIX_MAP_PTABLE(level), pte, MT_KERNEL_MEM);
}

static void ptable_put(bool use_fixmap)
{
    if (use_fixmap)
    {
        spin_unlock(&ptable_fixmap_lock);
    }
}

static void ptable_entry_set(phys_addr_t pte, int level, int idx, uint64_t value)
{
    bool use_fixmap;
    uint64_t *table = ptable_get(pte, level, &use_fixmap);
    table[idx] = value;
    ptable_put(use_fixmap);
}

static uint64_t ptable_entry_get(phys_addr_t pte, int level, int idx)
{
    bool use_fixmap;
    uint64_t value;
    uint64_t *table = ptable_get(pte, level, &use_fixmap);
    value = table[idx];
    ptable_put(use_fixmap);
    return value;
}

static void __ptable_clear(phys_addr_t pte, int level)
{
    bool use_fixmap;
    uint64_t *table = ptable_get(pte, level, &use_fixmap);
    memset(table, 0, PAGE_SIZE);
    ptable_put(use_fixmap);
}

static void ptable_clear(struct mm *mm, phys_addr_t pte, int level)
{
    irq_flags_t flags;

    spin_lock_irqsave(&mm->ptable_lock, flags);
    __ptable_clear(pte, level);
    spin_unlock_irqrestore(&mm->ptable_lock, flags);
}

static int ptable_valid_num(phys_addr_t pte, int level)
{
    int i, count = 0;
    bool use_fixmap;
    uint64_t *table = ptable_get(pte, level, &use_fixmap);

    for (i = 0; i < 512; i++)
    {
        if (table[i] & ENTRY_TYPE_USED)
        {
            count++;
        }
    }
    ptable_put(use_fixmap);
    return count;
}

static phys_addr_t v2p_by_pvoffset(virt_addr_t va, phys_addr_t pvoffset)
{
    phys_addr_t pa = va + pvoffset;
    return pa;
}

static virt_addr_t p2v_by_pvoffset(phys_addr_t pa, phys_addr_t pvoffset)
{
    return pa - pvoffset;
}

static phys_addr_t mmu_v2p_r(struct mm *mm, virt_addr_t v_addr)
{
    phys_addr_t pte = mm->mmu_table_base;
    uint64_t value;
    int level;
    int off;

#if V2P_BY_REG
    phys_addr_t pa_page = 0;
#ifdef __aarch64__
    ats1e1r((unsigned long)v_addr);

    if (!(PAR_F_MASK & read_par_el1()))
    {
        pa_page = read_par_el1() & PAR_ADDR_MASK & ~PAGE_SIZE_MASK;
        return pa_page + (v_addr & PAGE_SIZE_MASK);
    }
#else
    write_ats1cpr((unsigned long)v_addr);
    if (!(PAR_F_MASK & read64_par()))
    {
        pa_page = read64_par() & PAR_ADDR_MASK & ~PAGE_SIZE_MASK;
        return pa_page + (v_addr & PAGE_SIZE_MASK);
    }
#endif
#endif
    irq_flags_t flags;
    spin_lock_irqsave(&mm->ptable_lock, flags);
    for (level = mm->first_level; level < MAX_TABLE_LEVEL + 1; level++)
    {
        off = GET_TABLE_OFF(level, v_addr);
        value = ptable_entry_get(pte, level, off);
        if (((value & ENTRY_TYPE_MASK) == ENTRY_TYPE_TABLE) && (level < MAX_TABLE_LEVEL))
        {
            pte = value & ENTRY_ADDRESS_MASK;
        }
        else if (value & ENTRY_TYPE_USED)
        {
            spin_unlock_irqrestore(&mm->ptable_lock, flags);
            return (value & ENTRY_ADDRESS_MASK) + (v_addr - LEVEL_SIZE_ALIGN(level, v_addr));
        }
        else
        {
            spin_unlock_irqrestore(&mm->ptable_lock, flags);
            return 0ULL;
        }
    }
    spin_unlock_irqrestore(&mm->ptable_lock, flags);
    return 0ULL;
}

static uintptr_t mmu_get_attr_r(struct mm *mm, virt_addr_t v_addr)
{
    phys_addr_t pte = mm->mmu_table_base;
    uint64_t value;
    int level;
    int off;
    irq_flags_t flags;
    spin_lock_irqsave(&mm->ptable_lock, flags);
    for (level = mm->first_level; level < MAX_TABLE_LEVEL + 1; level++)
    {
        off = GET_TABLE_OFF(level, v_addr);
        value = ptable_entry_get(pte, level, off);
        if (((value & ENTRY_TYPE_MASK) == ENTRY_TYPE_TABLE) && (level < MAX_TABLE_LEVEL))
        {
            pte = (value & ENTRY_ADDRESS_MASK);
        }
        else if (value & ENTRY_TYPE_USED)
        {
            spin_unlock_irqrestore(&mm->ptable_lock, flags);
            return value & ENTRY_ATTRIB_MASK;
        }
        else
        {
            spin_unlock_irqrestore(&mm->ptable_lock, flags);
            return 0;
        }
    }
    spin_unlock_irqrestore(&mm->ptable_lock, flags);
    return 0;
}

static int map_single_entry_r(struct mm *mm, int level, virt_addr_t va, phys_addr_t pa,
                              uint64_t attr)
{
    int level_final = level;
    phys_addr_t pte = mm->mmu_table_base;
    uint64_t value;
    phys_addr_t page;
    int off;
    irq_flags_t flags;
    if (!IS_LEVEL_ALIGN(level_final, va))
    {
        return -1;
    }
    if (!IS_LEVEL_ALIGN(level_final, pa))
    {
        return -1;
    }
    spin_lock_irqsave(&mm->ptable_lock, flags);
    for (level = mm->first_level; level < level_final; level++)
    {
        off = GET_TABLE_OFF(level, va);
        value = ptable_entry_get(pte, level, off);
        if (!(value & ENTRY_TYPE_USED))
        {
            /* 创建下级表 */
            page = ptable_alloc();
            if (!page)
            {
                spin_unlock_irqrestore(&mm->ptable_lock, flags);
                return -1;
            }
            __ptable_clear(page, level + 1);
            ptable_entry_set(pte, level, off, TABLE_DESC(page));
            pte = page;
            continue;
        }
        if ((value & ENTRY_TYPE_MASK) == ENTRY_TYPE_BLOCK)
        {
            spin_unlock_irqrestore(&mm->ptable_lock, flags);
            /* is block! error! */
            return -1;
        }

        pte = value & ENTRY_ADDRESS_MASK;
    }
    off = GET_TABLE_OFF(level, va);
    if (level == MAX_TABLE_LEVEL)
    {
        ptable_entry_set(pte, level, off, PAGE_DESC(attr, pa));
    }
    else
    {
        ptable_entry_set(pte, level, off, BLOCK_DESC(attr, pa));
    }
    spin_unlock_irqrestore(&mm->ptable_lock, flags);
    return 0;
}

static size_t ummap_single_entry_r(struct mm *mm, virt_addr_t va, size_t size, bool is_unmap,
                                   uint64_t new_attr)
{
    int level = 0;
    phys_addr_t pte_lv[MAX_TABLE_LEVEL + 1];
    uint64_t value;
    int off;
    uint64_t attr;
    phys_addr_t phy;
    irq_flags_t flags;
    if (!IS_LEVEL_ALIGN(MAX_TABLE_LEVEL, va))
    {
        return -1;
    }

    if (!IS_LEVEL_ALIGN(MAX_TABLE_LEVEL, size))
    {
        return -1;
    }
    pte_lv[mm->first_level] = mm->mmu_table_base;
    spin_lock_irqsave(&mm->ptable_lock, flags);
    for (level = mm->first_level; level < MAX_TABLE_LEVEL + 1; level++)
    {
        off = GET_TABLE_OFF(level, va);
        value = ptable_entry_get(pte_lv[level], level, off);
        if (!(value & ENTRY_TYPE_USED))
        {
            spin_unlock_irqrestore(&mm->ptable_lock, flags);
            /* no maped */
            return 0;
        }

        if ((value & ENTRY_TYPE_MASK) == ENTRY_TYPE_TABLE && level != MAX_TABLE_LEVEL) /* table */
        {
            pte_lv[level + 1] = (value & ENTRY_ADDRESS_MASK);
        }
        else
        {
            /* 如果当前的地址是在块映射中,且并非解映射整个块则需要拆块 */
            if ((value & ENTRY_TYPE_MASK) == ENTRY_TYPE_BLOCK &&
                (size < LEVEL_SIZE(level) || !IS_LEVEL_ALIGN(level, va)))
            {
                attr = value & ENTRY_ATTRIB_MASK;
                phy = value & ENTRY_ADDRESS_MASK;

                /* 创建下级表 */
                phys_addr_t page = ptable_alloc();
                if (!page)
                {
                    spin_unlock_irqrestore(&mm->ptable_lock, flags);
                    return 0;
                }
                __ptable_clear(page, level + 1);
                for (int i = 0; i < 512; i++)
                {
                    ptable_entry_set(page, level + 1, i,
                                            (level + 1) == MAX_TABLE_LEVEL
                                                ? PAGE_DESC(attr, phy + i * PAGE_SIZE)
                                                : BLOCK_DESC(attr, phy + i * LEVEL_SIZE(level + 1)));
                }
                ptable_entry_set(pte_lv[level], level, off, TABLE_DESC(page));
                /* 继续往下走 */
                pte_lv[level + 1] = page;
            }
            else
            {
                if (is_unmap)
                {
                    /* 解除当前映射 */
                    ptable_entry_set(pte_lv[level], level, off, 0);

                    /* 如果当前为空表 则再减一次引用计数 */
                    if (ptable_valid_num(pte_lv[level], level) == 0)
                    {
                        ptable_free(pte_lv[level]);

                        /* 如果当前表已为空表则需要对上级表的引用计数-1并删除表 */
                        int __release_level = level - 1;
                        while (__release_level >= mm->first_level)
                        {
                            ptable_entry_set(pte_lv[__release_level], __release_level,
                                                    GET_TABLE_OFF(__release_level, va), 0);

                            /* 如果当前表已为空表则需要对上级表的引用计数-1并删除表 */
                            if ((ptable_valid_num(pte_lv[__release_level], __release_level) ==
                                 0) &&
                                (__release_level != mm->first_level))
                            {
                                ptable_free(pte_lv[__release_level]);
                            }
                            else
                            {
                                break;
                            }
                            __release_level--;
                        }
                    }
                }
                else
                {
                    /* 修改当前映射 */
                    ptable_entry_set(pte_lv[level], level, off,
                                            level == MAX_TABLE_LEVEL
                                                ? PAGE_DESC(new_attr, value & ENTRY_ADDRESS_MASK)
                                                : BLOCK_DESC(new_attr, value & ENTRY_ADDRESS_MASK));
                }
                spin_unlock_irqrestore(&mm->ptable_lock, flags);
                return LEVEL_SIZE(level);
            }
        }
    }
    spin_unlock_irqrestore(&mm->ptable_lock, flags);
    return 0;
}

static int mmu_unmap_r(struct mm *mm, virt_addr_t va, size_t size)
{
    size_t unmap_size = 0;
    while (size)
    {
        unmap_size = ummap_single_entry_r(mm, va, size, true, 0);
        mmu_table_flush(va);
        if (unmap_size == 0)
        {
            return 0;
        }
        size -= unmap_size;
        va += unmap_size;
    }
    return 0;
}

static int mmu_map_r(struct mm *mm, virt_addr_t va, phys_addr_t pa, size_t size, uint64_t attr)
{
    size_t i;
    size_t count;
    int level;
    int ret;

    level = GET_MAP_LEVEL(va, pa, size);

    count = size >> LEVEL_SIZE_SHIFT(level);

    if (!IS_LEVEL_ALIGN(level, pa))
    {
        return -1;
    }
    if (!IS_LEVEL_ALIGN(level, va))
    {
        return -1;
    }
    for (i = 0; i < count; i++)
    {
        ret = map_single_entry_r(mm, level, va, pa, attr);
        mmu_table_flush(va);
        va += LEVEL_SIZE(level);
        pa += LEVEL_SIZE(level);
        if (ret != 0)
        {
            return ret;
        }
    }
    return 0;
}

static void mmu_table_flush(virt_addr_t vaddr)
{
#ifdef __aarch64__
    dsbishst();
    if (vaddr == 0)
    {
        tlbivmalle1();
    }
    else
    {
        if (IS_ENABLED(CONFIG_ENABLE_MMU_FLASH_TLB_ALL))
        {
            tlbivmalle1(); // Flush all TLB entries in EL1
        }
        else
        {
            tlbivaae1is(TLBI_ADDR(vaddr));
        }
    }
    dsbish();
    isb();
#else
    asm volatile("dsb ishst" ::: "memory");
    if (vaddr == 0)
    {
        sysreg_write(0, TLBIALL);
    }
    else
    {
        if (IS_ENABLED(CONFIG_ENABLE_MMU_FLASH_TLB_ALL))
        {
            sysreg_write(0, TLBIALL);
        }
        else
        {
            sysreg_write(TLBI_ADDR(vaddr), TLBIMVAAIS);
        }
    }

    sysreg_write(0, BPIALLIS);
    asm volatile("dsb ish" ::: "memory");
    asm volatile("isb" ::: "memory");
#endif
}

static uint64_t attr_2_mmu(uint64_t attr)
{
    uint64_t return_attr;

    if (attr & MT_NO_ACCESS)
    {
        return_attr = 0;
    }
    else
    {
        return_attr = LOWER_ATTRS(ACCESS_FLAG);
    }

    switch (MT_TYPE(attr))
    {
    case MT_DEV:
        return_attr |= UPPER_ATTRS(PXN);
        return_attr |= LOWER_ATTRS(MEM_DEVICE_INDEX);
        break;
    case MT_NON_CACHEABLE:
        return_attr |= LOWER_ATTRS(MEM_NORMAL_NC_INDEX);
        break;
    case MT_WRITE_THROUGHOUT:
        return_attr |= LOWER_ATTRS(MEM_NORMAL_WT_INDEX);
        break;
    case MT_MEMORY:
    default:
        return_attr |= LOWER_ATTRS(MEM_NORMAL_INDEX);
        break;
    }
    if (MT_PAS(attr) == MT_NSECURE)
    {
        return_attr |= LOWER_ATTRS(NS);
    }
    switch (MT_SHAREABILITY(attr))
    {
    case MT_SHAREABILITY_NSH:
        return_attr |= LOWER_ATTRS(NSH);
        break;
    case MT_SHAREABILITY_OSH:
        return_attr |= LOWER_ATTRS(OSH);
        break;
    case MT_SHAREABILITY_ISH:
    default:
        return_attr |= LOWER_ATTRS(ISH);
        break;
    }
    if (attr & MT_USER)
    {
        return_attr |= LOWER_ATTRS(AP_ACCESS_UNPRIVILEGED | NON_GLOBAL);
        if (attr & MT_EXECUTE_NEVER)
        {
            return_attr |= UPPER_ATTRS(UXN);
        }
    }
    else
    {
        return_attr |= LOWER_ATTRS(AP_NO_ACCESS_UNPRIVILEGED);
    }
    if (attr & MT_EXECUTE_NEVER)
    {
        return_attr |= UPPER_ATTRS(PXN);
    }
    if (attr & MT_RW)
    {
        return_attr |= LOWER_ATTRS(AP_RW);
    }
    else
    {
        return_attr |= LOWER_ATTRS(AP_RO);
    }
    return return_attr;
}

virt_addr_t arm_alloc_vaddr(struct mm *mm, size_t page_count)
{
    return arm_alloc_vaddr_with_min(mm, page_count, 0);
}

static virt_addr_t arm_alloc_vaddr_with_min(struct mm *mm, size_t page_count, virt_addr_t map_min)
{
    virt_addr_t va, find_va = 0;
    int n = 0;
    virt_addr_t va_start, va_end;
    if (mm->mm_region_type == MM_REGION_TYPE_KERNEL)
    {
        va_start = map_min ? map_min : KERNEL_SPACE_START;
        va_end = (((UL(1) << ARCH_VADDRESS_WIDTH_BITS) - 1) + KERNEL_SPACE_START);
    }
    else
    {
        va_start = map_min ? map_min : USER_SPACE_START;

        if (va_start < MMAP_START_VADDR)
        {
            va_start = MMAP_START_VADDR;
        }
        va_end = USER_SPACE_END;
    }
    for (va = va_start; va < va_end; va += PAGE_SIZE)
    {
        /* skip kernel linear map table */
        if (va == KERNEL_SPACE_START)
        {
            va = VA_RAM_END;
            n = 0;
            find_va = 0;
            continue;
        }
        if (mmu_v2p_r(mm, va) != 0ULL)
        {
            n = 0;
            find_va = 0;
            continue;
        }
        if (!find_va)
        {
            find_va = va;
        }
        n++;
        if (n >= page_count)
        {
            return find_va;
        }
    }
    return -1;
}

static int arm_mmu_init(struct mm *mm)
{
    if (mm)
    {
        mm->page_size = PAGE_SIZE;
        mm->page_size_shift = PAGE_SIZE_SHIFT;
#ifdef __aarch64__
        mm->first_level = 0;
        if (mm->mm_region_type == MM_REGION_TYPE_KERNEL)
        {
            if (gp_kernel_mm == NULL)
            {
                gp_kernel_mm = mm;
                mm->mmu_table_base =
                    v2p_by_pvoffset((uintptr_t)&_kernel_mmu_table0, gp_kernel_mm->pv_offset);
            }
            else
            {
                KLOG_E("can not create kernel mm");
            }
        }
        else if (mm->mm_region_type == MM_REGION_TYPE_USER)
        {
            mm->pv_offset = gp_kernel_mm->pv_offset;
            mm->mmu_table_base = ptable_alloc();
        }
        ptable_clear(mm, mm->mmu_table_base, 0);
        if (mm->mm_region_type == MM_REGION_TYPE_KERNEL)
        {
            _kernel_mmu_table0[GET_TABLE_OFF(0, FIX_MAP_START)] =
                TABLE_DESC(v2p_by_pvoffset((virt_addr_t)g_fix_map_level1, gp_kernel_mm->pv_offset));
        }
#else
        if (mm->mm_region_type == MM_REGION_TYPE_KERNEL)
        {
            if (gp_kernel_mm == NULL)
            {
                gp_kernel_mm = mm;
                mm->first_level = 2;
                mm->mmu_table_base =
                    v2p_by_pvoffset((uintptr_t)&_kernel_mmu_table0, gp_kernel_mm->pv_offset);
            }
            else
            {
                KLOG_E("can not create kernel mm");
            }
        }
        else if (mm->mm_region_type == MM_REGION_TYPE_USER)
        {
            mm->first_level = 1;
            mm->pv_offset = gp_kernel_mm->pv_offset;
            mm->mmu_table_base = ptable_alloc();
        }
        ptable_clear(mm, mm->mmu_table_base, 0);
        if (mm->mm_region_type == MM_REGION_TYPE_KERNEL)
        {
            _kernel_mmu_table0[GET_TABLE_OFF(2, FIX_MAP_START)] =
                TABLE_DESC(v2p_by_pvoffset((virt_addr_t)g_fix_map_level3, gp_kernel_mm->pv_offset));
        }
#endif

        return !mm->mmu_table_base;
    }
    return -1;
}

static int arm_mmu_deinit(struct mm *mm)
{
    if (mm->mm_region_type == MM_REGION_TYPE_USER)
    {
        ptable_free(mm->mmu_table_base);
    }
    return 0;
}

static int arm_mmu_switch_space(struct mm *from, struct mm *to)
{
    if (to == NULL && from == NULL)
    {
        return -1;
    }
    if (to == NULL)
    {
        if (from->mm_region_type == MM_REGION_TYPE_USER)
        {
#ifdef __aarch64__
            write_ttbr0_el1(0);
#else
            sysreg_write_64(0, TTBR0_64);
#endif
            return 0;
        }
        return -1;
    }
    if (from != NULL)
    {
        if (from->mmu_table_base == to->mmu_table_base)
        {
            return 0;
        }
    }
    switch (to->mm_region_type)
    {
    case MM_REGION_TYPE_KERNEL:
#ifdef __aarch64__
        write_ttbr1_el1(to->mmu_table_base);
#else
        sysreg_write_64(to->mmu_table_base, TTBR1_64);

#endif
        mmu_table_flush(0);
        break;
    case MM_REGION_TYPE_USER:
#ifdef __aarch64__
        write_ttbr0_el1(to->mmu_table_base | to->asid << 48);
#else
        sysreg_write_64(to->mmu_table_base | to->asid << 48, TTBR0_64);
#endif
        mmu_table_flush(0);
        break;
    case MM_REGION_TYPE_HYPER:
    default:
        return -1;
    }
    return 0;
}

int kmap(virt_addr_t va, phys_addr_t pa, size_t size, uint64_t attr)
{
    attr = attr_2_mmu(attr);
    return mmu_map_r(gp_kernel_mm, va, pa, size, attr);
}

static int arm_mmu_map(struct mm *mm, struct mm_region *region)
{
    uint64_t attr;
    int ret;
    if (mm == NULL || region == NULL)
    {
        return -1;
    }
    attr = attr_2_mmu(region->mem_attr);

    if (region->flags & MAP_FIXED)
    {
        region->virtual_address = region->map_min;
    }
    else
    {
        if (!region->virtual_address)
        {
            region->virtual_address =
                arm_alloc_vaddr_with_min(mm, region->region_page_count, region->map_min);
        }
    }
    ret = mmu_map_r(mm, region->virtual_address, region->physical_address,
                    region->region_page_count * mm->page_size, attr);

    return ret;
}
static int arm_mmu_modify(struct mm *mm, struct mm_region *region)
{
    uint64_t attr;
    int ret;
    attr = attr_2_mmu(region->mem_attr);

    size_t modify_size = 0;
    size_t size = region->region_page_count * mm->page_size;
    virt_addr_t va = region->virtual_address;
    while (size)
    {
        modify_size = ummap_single_entry_r(mm, va, size, false, attr);
        mmu_table_flush(va);
        if (modify_size == 0)
        {
            return -1;
        }
        size -= modify_size;
        va += modify_size;
    }
    return 0;
}

static int arm_mmu_unmap(struct mm *mm, struct mm_region *region)
{
    int ret = mmu_unmap_r(mm, region->virtual_address, region->region_page_count * mm->page_size);
    return ret;
}

static phys_addr_t arm_mmu_v2p(struct mm *mm, virt_addr_t v_addr)
{
    if (v_addr < VA_RAM_END && v_addr > VA_RAM_BASE)
    {
        return v2p_by_pvoffset(v_addr, gp_kernel_mm->pv_offset);
    }
    return mmu_v2p_r(mm, v_addr);
}

static int arm_mmu_apply_change(struct mm *mm)
{
    if (mm == NULL)
        return -1;
    return 0;
}

static void tables_print_r(virt_addr_t table_base_va, phys_addr_t table_base,
                                  unsigned int table_entries, unsigned int level,
                                  int (*output)(const char *str));
static int arm_mmu_print_table(struct mm *mm, int (*output)(const char *str))
{
    virt_addr_t table_base_va = 0;
    irq_flags_t flags;
    if (mm->mm_region_type == MM_REGION_TYPE_KERNEL)
    {
        table_base_va = KERNEL_SPACE_START;
    }
    spin_lock_irqsave(&mm->ptable_lock, flags);
    tables_print_r(table_base_va, mm->mmu_table_base, 512, mm->first_level, output);
    spin_unlock_irqrestore(&mm->ptable_lock, flags);
    return 0;
}

static struct mmu_ops arm_mmu_ops = {.init = arm_mmu_init,
                                     .switch_space = arm_mmu_switch_space,
                                     .map = arm_mmu_map,
                                     .modify = arm_mmu_modify,
                                     .unmap = arm_mmu_unmap,
                                     .v2p = arm_mmu_v2p,
                                     .valloc = arm_alloc_vaddr,
                                     .apply_change = arm_mmu_apply_change,
                                     .print_table = arm_mmu_print_table,
                                     .deinit = arm_mmu_deinit};

struct mmu_ops *arch_get_mmu_ops(void)
{
    return &(arm_mmu_ops);
}

#define TB_OUT(...)                                                                                \
    snprintf(outbuffer, sizeof(outbuffer), ##__VA_ARGS__);                                         \
    output(outbuffer);

#define invalid_descriptors_ommited "%s(%d invalid descriptors omitted)\n"

static const char level_spacers[][16] = {"[LV0] ", "  [LV1] ", "    [LV2] ", "      [LV3] "};

static void desc_print(uint64_t desc, int (*output)(const char *str))
{
    char outbuffer[128];
    uint64_t mem_type_index = ATTR_INDEX_GET(desc);

    if (mem_type_index == MEM_NORMAL_INDEX)
    {
        TB_OUT("MEM");
    }
    else if (mem_type_index == MEM_NORMAL_NC_INDEX)
    {
        TB_OUT("NC");
    }
    else if (mem_type_index == MEM_DEVICE_INDEX)
    {
        TB_OUT("DEV");
    }
    else
    {
        TB_OUT("WT");
    }

    TB_OUT(((desc & LOWER_ATTRS(AP_RO)) != 0ULL) ? "-RO" : "-RW");
    /* Only check one of PXN and UXN, the other one is the same. */
    TB_OUT(((desc & UPPER_ATTRS(PXN)) != 0ULL) ? "-XN" : "-EXEC");
    /*
     * Privileged regions can only be accessed from EL1, user
     * regions can be accessed from EL1 and EL0.
     */
    TB_OUT(((desc & LOWER_ATTRS(AP_ACCESS_UNPRIVILEGED)) != 0ULL) ? "-USER" : "-PRIV");

    TB_OUT(((LOWER_ATTRS(NS) & desc) != 0ULL) ? "-NS" : "-S");

#ifdef __aarch64__
    /* Check Guarded Page bit */
    if ((desc & GP) != 0ULL)
    {
        TB_OUT("-GP");
    }
#endif
}

static void tables_print_r(virt_addr_t table_base_va, phys_addr_t table_base,
                                  unsigned int table_entries, unsigned int level,
                                  int (*output)(const char *str))
{
    uint64_t desc;
    virt_addr_t table_idx_va = table_base_va;
    unsigned int table_idx = 0U;
    size_t level_size = LEVEL_SIZE(level);
    char outbuffer[128];

    /*
     * Keep track of how many invalid descriptors are counted in a row.
     * Whenever multiple invalid descriptors are found, only the first one
     * is printed, and a line is added to inform about how many descriptors
     * have been omitted.
     */
    int invalid_row_count = 0;

    while (table_idx < table_entries)
    {
        desc = ptable_entry_get(table_base, level, table_idx);

        if (!(desc & ENTRY_TYPE_USED))
        {
            if (invalid_row_count == 0)
            {
                TB_OUT("%sVA:0x%lx size:0x%" PRIxPTR "\n", level_spacers[level], table_idx_va,
                       level_size);
            }
            invalid_row_count++;
        }
        else
        {
            if (invalid_row_count > 1)
            {
                TB_OUT(invalid_descriptors_ommited, level_spacers[level], invalid_row_count - 1);
            }
            invalid_row_count = 0;

            /*
             * Check if this is a table or a block. Tables are only
             * allowed in levels other than 3, but DESC_PAGE has the
             * same value as DESC_TABLE, so we need to check.
             */
            if (((desc & ENTRY_TYPE_MASK) == ENTRY_TYPE_TABLE) && (level < MAX_TABLE_LEVEL))
            {
                phys_addr_t addr_inner = (desc & ENTRY_ADDRESS_MASK);
                /*
                 * Do not print any PA for a table descriptor,
                 * as it doesn't directly map physical memory
                 * but instead points to the next translation
                 * table in the translation table walk.
                 */
                TB_OUT("%sVA:0x%lx size:0x%" PRIxPTR " (0x%" PRIx64 ")\n", level_spacers[level],
                       table_idx_va, level_size, addr_inner);

                tables_print_r(table_idx_va, addr_inner, TABLE_ENTRIES, level + 1U, output);
            }
            else
            {
                TB_OUT("%sVA:0x%lx PA:0x%" PRIx64 " size:0x%" PRIxPTR " ", level_spacers[level],
                       table_idx_va, (desc & ENTRY_ADDRESS_MASK), level_size);
                desc_print(desc, output);
                TB_OUT("\n");
            }
        }

        table_idx++;
        table_idx_va += level_size;
    }
    if (invalid_row_count > 1)
    {
        TB_OUT(invalid_descriptors_ommited, level_spacers[level], invalid_row_count - 1);
    }
#undef TB_OUT
}
