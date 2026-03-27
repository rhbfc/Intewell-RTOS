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
 * @file early_mmu.c
 * @brief AArch64 早期 MMU 初始化辅助实现。
 */

#include <arch_helpers.h>
#include <mmu.h>
#include <ttosMM.h>

/* 将整页清零，供早期页表构造复用。 */
static void page_clean (uintptr_t *page)
{
    int i = 0;
    for (i = 0; i < PAGE_SIZE / sizeof (uintptr_t); i++)
    {
        page[i] = 0;
    }
}

static uintptr_t __page_off     = 0;

/* 重置早期页表页分配偏移。 */
void reset_free_page (void)
{
    __page_off = 0;
}

uintptr_t get_free_page (uintptr_t pvoff)
{
    uintptr_t        __init_page_pa = read_ttbr0_el1 ();
    uintptr_t        page;
    if(__page_off == 0)
    {
        __page_off = 2;/* 0、1 号页分别保留给 ttbr0 和 ttbr1 */
    }
    __page_off++;

    page = __init_page_pa + (__page_off - 1) * PAGE_SIZE;
    /* 最多0x80个页 */
    if ((__page_off - 1) * PAGE_SIZE == 0x80000)
    {
        while (1)
            ;
    }
    return page;
}

static int early_map_single_entry_r (uint64_t *lv0_tbl, int level, uintptr_t va,
                                     uint64_t pa, uint64_t attr,
                                     uintptr_t pvoff)
{
    int       level_final = level;
    uint64_t *cur_lv_tbl  = lv0_tbl;
    uintptr_t page;
    uint64_t  off;

    if (!IS_LEVEL_ALIGN (level_final, va))
    {
        return -1;
    }
    if (!IS_LEVEL_ALIGN (level_final, pa))
    {
        return -1;
    }
#ifdef __arm__
    if ((va & KERNEL_SPACE_START) == KERNEL_SPACE_START)
    {
        va &= KERNEL_SPACE_MASK;
        level = 2;
    }
    else
    {
        level = 1;
    }
#else
    level = 0;
#endif
    for (; level < level_final; level++)
    {
        off = GET_TABLE_OFF (level, va);
        if (!(cur_lv_tbl[off] & ENTRY_TYPE_USED))
        {
            page = get_free_page (pvoff);
            if (!page)
            {
                return -1;
            }
            page_clean ((uintptr_t *)page);

            cur_lv_tbl[off] = TABLE_DESC (page);
        }
        page = cur_lv_tbl[off];
        if ((page & ENTRY_TYPE_MASK) == ENTRY_TYPE_BLOCK)
        {
            /* is block! error! */
            return -1;
        }
        cur_lv_tbl = (uint64_t *)(uintptr_t)(page & ENTRY_ADDRESS_MASK);
    }
    off = GET_TABLE_OFF (level, va);

    if (level == MAX_TABLE_LEVEL)
    {
        cur_lv_tbl[off] = PAGE_DESC (attr, pa);
    }
    else
    {
        cur_lv_tbl[off] = BLOCK_DESC (attr, pa);
    }
    return 0;
}

int early_mmu_map_r (uint64_t *lv0_tbl, uintptr_t va, uint64_t pa, size_t size,
                     uint64_t attr, uintptr_t pvoff)
{
    size_t i;
    size_t count;
    int    level;
    int    ret;

    level = GET_MAP_LEVEL (va, pa, size);

    count = size >> LEVEL_SIZE_SHIFT (level);

    if (!IS_LEVEL_ALIGN (level, pa))
    {
        return -1;
    }
    if (!IS_LEVEL_ALIGN (level, va))
    {
        return -1;
    }
    for (i = 0; i < count; i++)
    {
        ret = early_map_single_entry_r (lv0_tbl, level, va, pa, attr, pvoff);
        va += LEVEL_SIZE (level);
        pa += LEVEL_SIZE (level);
        if (ret != 0)
        {
            return ret;
        }
    }
    return 0;
}

void mmu_mair_tcr_init (void)
{
    uint64_t val64;

    val64 = MAIR_ATTR_SET (ATTR_DEVICE_nGnRE, MEM_DEVICE_INDEX);
    val64 |= MAIR_ATTR_SET (ATTR_IWBWA_OWBWA_NTR, MEM_NORMAL_INDEX);
    val64 |= MAIR_ATTR_SET (ATTR_NON_CACHEABLE, MEM_NORMAL_NC_INDEX);
    val64 |= MAIR_ATTR_SET (ATTR_IWTWA_OWTWA_NTR, MEM_NORMAL_WT_INDEX);

    write_mair_el1 (val64);

    /* TCR_EL1 */
    val64 = ((UL (64) - ARCH_VADDRESS_WIDTH_BITS)
             << TCR_T0SZ_SHIFT)        /* t0sz 48bit */
            | (0x0UL << 6)             /* reserved */
            | (0x0UL << 7)             /* epd0 */
            | (TCR_RGN_INNER_WBNA)     /* t0 wb cacheable */
            | (TCR_RGN_OUTER_WBNA)     /* inner shareable */
            | (TCR_SH_OUTER_SHAREABLE) /* t0 outer shareable */
            | (TCR_TG0_4K)             /* t0 4K */
            | ((UL (64) - ARCH_VADDRESS_WIDTH_BITS)
               << TCR_T1SZ_SHIFT)       /* t1sz 48bit */
            | (TCR_A1_TTBR0_ASID)       /* define asid use ttbr0.asid */
            | (0x0UL << 23)             /* epd1 */
            | (TCR_RGN1_INNER_WBNA)     /* t1 inner wb cacheable */
            | (TCR_RGN1_OUTER_WBNA)     /* t1 outer wb cacheable */
            | (TCR_SH1_OUTER_SHAREABLE) /* t1 outer shareable */
            | (TCR_TG1_4K)              /* t1 4k */
            | ((read_id_aa64mmfr0_el1() & 0xf) << TCR_EL1_IPS_SHIFT) /* 001b 64GB PA */
            | (0x0UL << 35)                          /* reserved */
            | (TCR_ASID_16BIT)                       /* as: 0:8bit 1:16bit */
            | (0x0UL << 37)                          /* tbi0 */
            | (0x0UL << 38);                         /* tbi1 */
    write_tcr_el1 (val64);
}

extern int __image_start__;
extern int __end__;
extern void start_enbale_mmu(void);

extern uint64_t __attribute__ ((aligned (PAGE_SIZE))) g_fix_map_level1[512];
extern uint64_t __attribute__ ((aligned (PAGE_SIZE))) g_fix_map_level2[512];
extern uint64_t __attribute__ ((aligned (PAGE_SIZE))) g_fix_map_level3[512];
void       mmu_setup_early (uintptr_t pv_off)
{
    int       ret;
    uint64_t *tbl0, *tbl1;
    uintptr_t kernel_space_va = (uintptr_t)KERNEL_SPACE_START;
    uint64_t  kernel_space_pa = (uintptr_t)kernel_space_va + pv_off;
    uint64_t mapsize = (uint64_t)&__end__ - (uint64_t)&__image_start__;

    mapsize = PAGE_ALIGN (mapsize);

    tbl0 = (uint64_t *)(uintptr_t)(kernel_space_pa);
    tbl1 = (uint64_t *)(((uintptr_t)tbl0) + 0x1000);

    write_ttbr0_el1 ((uintptr_t)tbl0);
    write_ttbr1_el1 ((uintptr_t)tbl1);

    /* clean the first two pages */
    page_clean ((uintptr_t *)tbl0);
    page_clean ((uintptr_t *)tbl1);

    ret = early_mmu_map_r (tbl1, kernel_space_va, kernel_space_pa, mapsize,
                           MEM_NORMAL_KRWX_ATTRS, pv_off);

    if (ret != 0)
    {
        while (1)
            ;
    }

    page_clean ((uintptr_t *)g_fix_map_level1);
    page_clean ((uintptr_t *)g_fix_map_level2);
    page_clean ((uintptr_t *)g_fix_map_level3);
    tbl1[GET_TABLE_OFF (0, FIX_MAP_START)] = TABLE_DESC (g_fix_map_level1);
    g_fix_map_level1[GET_TABLE_OFF (1, FIX_MAP_START)] = TABLE_DESC (g_fix_map_level2);
    g_fix_map_level2[GET_TABLE_OFF (2, FIX_MAP_START)] = TABLE_DESC (g_fix_map_level3);
    g_fix_map_level3[FIX_MAP_SELF] = PAGE_DESC(MEM_NORMAL_KRW_ATTRS, g_fix_map_level3);

    ret = early_mmu_map_r (tbl0, PAGE_ALIGN_DOWN((virt_addr_t)&start_enbale_mmu), PAGE_ALIGN_DOWN((phys_addr_t)&start_enbale_mmu), PAGE_SIZE,
                           MEM_NORMAL_KRWX_ATTRS, pv_off);
    if (ret != 0)
    {
        while (1)
            ;
    }
    reset_free_page();
}

void ap_mmu_setup_early (uintptr_t pv_off)
{
    int       ret;
    uintptr_t kernel_space_va = (uintptr_t)0xFFFF000000000000;
    uint64_t  kernel_space_pa = (uintptr_t)kernel_space_va + pv_off;

    write_ttbr0_el1 ((uintptr_t)kernel_space_pa);

    /* clean the first two pages */
    page_clean ((uintptr_t *)kernel_space_pa);

    ret = early_mmu_map_r ((uint64_t *)kernel_space_pa, PAGE_ALIGN_DOWN((virt_addr_t)&start_enbale_mmu), PAGE_ALIGN_DOWN((phys_addr_t)&start_enbale_mmu), PAGE_SIZE,
                           MEM_NORMAL_KRWX_ATTRS, pv_off);
    if (ret != 0)
    {
        while (1)
            ;
    }
}
