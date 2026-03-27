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

#ifndef __MMU_H__
#define __MMU_H__

#include <commonTypes.h>
#include <stdint.h>
#include <system/const.h>

#ifdef __aarch64__
#define ARCH_ADDRESS_WIDTH_BITS UL(64)
#define ARCH_VADDRESS_WIDTH_BITS UL(48)
#define USER_SPACE_START UL(0x1000)
#define USER_SPACE_END UL(0xffffffffffff)
#else
#define ARCH_ADDRESS_WIDTH_BITS UL(32)
#define ARCH_VADDRESS_WIDTH_BITS UL(30)
#define USER_SPACE_START UL(0)
#define USER_SPACE_END UL(KERNEL_SPACE_START - 1)
#endif

#define KERNEL_SPACE_START UL(CONFIG_KERNEL_SPACE_START)
#define ZONE_NORMAL_START KERNEL_SPACE_START

#define ZONE_NORMAL_SIZE (768 * 1024 * 1024)
#define ZONE_NORMAL_END (KERNEL_SPACE_START + ZONE_NORMAL_SIZE - 1)

#define KERNEL_SPACE_END UINTPTR_MAX

/* 最后一个表可映射的区域 */
#define FIX_MAP_START (KERNEL_SPACE_END - (4096 * 512) + 1)

#define FIX_MAP_SELF 0
#define FIX_MAP_EARLYCON 1
#define FIX_MAP_PTABLE(level) (2 + (level))
#define FIX_MAP_POOL_START (FIX_MAP_PTABLE(3) + 1) // max level is 3

#define FIXMAP_SETSIZE 512

typedef struct fixmap_set_t
{
    unsigned long __bits[FIXMAP_SETSIZE / sizeof(long)];
} fixmap_set_t;

#define __FIXMAP_op_S(i, size, set, op)                                                            \
    ((i) / 8U >= (size) ? 0                                                                        \
                        : (((unsigned long *)(set))[(i) / 8 / sizeof(long)] op(                    \
                              1UL << ((i) % (8 * sizeof(long))))))

#define FIXMAP_SET_S(i, size, set) __FIXMAP_op_S(i, size, set, |=)
#define FIXMAP_CLR_S(i, size, set) __FIXMAP_op_S(i, size, set, &= ~)
#define FIXMAP_ISSET_S(i, size, set) __FIXMAP_op_S(i, size, set, &)

#define __FIXMAP_op_func_S(func, op)                                                               \
    static __inline void __FIXMAP_##func##_S(size_t __size, fixmap_set_t *__dest,                  \
                                             const fixmap_set_t *__src1,                           \
                                             const fixmap_set_t *__src2)                           \
    {                                                                                              \
        size_t __i;                                                                                \
        for (__i = 0; __i < __size / sizeof(long); __i++)                                          \
            ((unsigned long *)__dest)[__i] =                                                       \
                ((unsigned long *)__src1)[__i] op((unsigned long *)__src2)[__i];                   \
    }

__FIXMAP_op_func_S(AND, &) __FIXMAP_op_func_S(OR, |) __FIXMAP_op_func_S(XOR, ^)

#define FIXMAP_AND_S(a, b, c, d) __FIXMAP_AND_S(a, b, c, d)
#define FIXMAP_OR_S(a, b, c, d) __FIXMAP_OR_S(a, b, c, d)
#define FIXMAP_XOR_S(a, b, c, d) __FIXMAP_XOR_S(a, b, c, d)

#define FIXMAP_ZERO_S(size, set) memset(set, 0, size)
#define FIXMAP_EQUAL_S(size, set1, set2) (!memcmp(set1, set2, size))

#define FIXMAP_SET(i, set) FIXMAP_SET_S(i, sizeof(fixmap_set_t), set)
#define FIXMAP_CLR(i, set) FIXMAP_CLR_S(i, sizeof(fixmap_set_t), set)
#define FIXMAP_ISSET(i, set) FIXMAP_ISSET_S(i, sizeof(fixmap_set_t), set)
#define FIXMAP_AND(d, s1, s2) FIXMAP_AND_S(sizeof(fixmap_set_t), d, s1, s2)
#define FIXMAP_OR(d, s1, s2) FIXMAP_OR_S(sizeof(fixmap_set_t), d, s1, s2)
#define FIXMAP_XOR(d, s1, s2) FIXMAP_XOR_S(sizeof(fixmap_set_t), d, s1, s2)
#define FIXMAP_ZERO(set) FIXMAP_ZERO_S(sizeof(fixmap_set_t), set)
#define FIXMAP_EQUAL(s1, s2) FIXMAP_EQUAL_S(sizeof(fixmap_set_t), s1, s2)

#ifdef __arm__
#define FIX_MAP_USER_HLEPER ((0xffff0000 - FIX_MAP_START) / 4096)
#endif

#define KERNEL_SPACE_MASK (~KERNEL_SPACE_START)

/* XN: Translation regimes that support one VA range (EL2 and EL3). */
#define XN (ULL(1) << 2)
/* UXN, PXN: Translation regimes that support two VA ranges (EL1&0). */
#define UXN (ULL(1) << 2)
#define PXN (ULL(1) << 1)
#define CONT_HINT (ULL(1) << 0)
#define UPPER_ATTRS(x) (((x)&ULL(0x7)) << 52)

/*
 * The following definitions must all be passed to the LOWER_ATTRS() macro to
 * get the right bitmask.
 */
#define NON_GLOBAL (U(1) << 9)
#define ACCESS_FLAG (U(1) << 8)
#define NSH (U(0x0) << 6)
#define OSH (U(0x2) << 6)
#define ISH (U(0x3) << 6)

#define AP2_SHIFT U(0x7)
#define AP2_RO ULL(0x1)
#define AP2_RW ULL(0x0)

#define AP1_SHIFT U(0x6)
#define AP1_ACCESS_UNPRIVILEGED ULL(0x1)
#define AP1_NO_ACCESS_UNPRIVILEGED ULL(0x0)
#define AP1_RES1 ULL(0x1)

#define AP_RO (AP2_RO << 5)
#define AP_RW (AP2_RW << 5)
#define AP_ACCESS_UNPRIVILEGED (AP1_ACCESS_UNPRIVILEGED << 4)
#define AP_NO_ACCESS_UNPRIVILEGED (AP1_NO_ACCESS_UNPRIVILEGED << 4)
#define AP_ONE_VA_RANGE_RES1 (AP1_RES1 << 4)
#define NS (U(0x1) << 3)
#define EL3_S1_NSE (U(0x1) << 9)

#define LOWER_ATTRS(x) (((x)&U(0xfff)) << 2)

#define ATTR_IWTWA_OWTWA_INDEX ULL(0x3)
#define ATTR_NON_CACHEABLE_INDEX ULL(0x2)
#define ATTR_DEVICE_nGnRE_INDEX ULL(0x1)
#define ATTR_IWBWA_OWBWA_NTR_INDEX ULL(0x0)

// #define ATTR_NON_CACHEABLE_INDEX	ULL(0x2)
// #define ATTR_DEVICE_INDEX		ULL(0x1)
// #define ATTR_IWBWA_OWBWA_NTR_INDEX	ULL(0x0)

#define MEM_NORMAL_INDEX ATTR_IWBWA_OWBWA_NTR_INDEX
#define MEM_DEVICE_INDEX ATTR_DEVICE_nGnRE_INDEX
#define MEM_NORMAL_WT_INDEX ATTR_IWTWA_OWTWA_INDEX
#define MEM_NORMAL_NC_INDEX ATTR_NON_CACHEABLE_INDEX

#define ENTRY_ATTRS(up, lower, index) (LOWER_ATTRS(lower | index) | UPPER_ATTRS(up))

#define MEM_NORMAL_KRW_ATTRS                                                                       \
    ENTRY_ATTRS(XN | PXN, ACCESS_FLAG | ISH | AP_RW | AP_NO_ACCESS_UNPRIVILEGED, MEM_NORMAL_INDEX)
#define MEM_NORMAL_KRWX_ATTRS                                                                      \
    ENTRY_ATTRS(0, ACCESS_FLAG | ISH | AP_RW | AP_NO_ACCESS_UNPRIVILEGED, MEM_NORMAL_INDEX)
#define MEM_NORMAL_KROX_ATTRS                                                                      \
    ENTRY_ATTRS(0, ACCESS_FLAG | ISH | AP_RO | AP_NO_ACCESS_UNPRIVILEGED, MEM_NORMAL_INDEX)

#define MEM_DEVICE_KRW_ATTRS                                                                       \
    ENTRY_ATTRS(XN | PXN, ACCESS_FLAG | ISH | AP_RW | AP_NO_ACCESS_UNPRIVILEGED, MEM_DEVICE_INDEX)
#define MEM_DEVICE_KRO_ATTRS                                                                       \
    ENTRY_ATTRS(XN | PXN, ACCESS_FLAG | ISH | AP_RO | AP_NO_ACCESS_UNPRIVILEGED, MEM_DEVICE_INDEX)

#define MEM_NORMAL_NC_KRW_ATTRS                                                                    \
    ENTRY_ATTRS(XN | PXN, ACCESS_FLAG | ISH | AP_RW | AP_NO_ACCESS_UNPRIVILEGED,                   \
                MEM_NORMAL_NC_INDEX)
#define MEM_NORMAL_NC_KRWX_ATTRS                                                                   \
    ENTRY_ATTRS(0, ACCESS_FLAG | ISH | AP_RW | AP_NO_ACCESS_UNPRIVILEGED, MEM_NORMAL_NC_INDEX)
#define MEM_NORMAL_NC_KROX_ATTRS                                                                   \
    ENTRY_ATTRS(0, ACCESS_FLAG | ISH | AP_RO | AP_NO_ACCESS_UNPRIVILEGED, MEM_NORMAL_NC_INDEX)

#define MEM_NORMAL_WT_KRW_ATTRS                                                                    \
    ENTRY_ATTRS(XN | PXN, ACCESS_FLAG | ISH | AP_RW | AP_NO_ACCESS_UNPRIVILEGED,                   \
                MEM_NORMAL_WT_INDEX)
#define MEM_NORMAL_WT_KRWX_ATTRS                                                                   \
    ENTRY_ATTRS(0, ACCESS_FLAG | ISH | AP_RW | AP_NO_ACCESS_UNPRIVILEGED, MEM_NORMAL_WT_INDEX)
#define MEM_NORMAL_WT_KROX_ATTRS                                                                   \
    ENTRY_ATTRS(0, ACCESS_FLAG | ISH | AP_RO | AP_NO_ACCESS_UNPRIVILEGED, MEM_NORMAL_WT_INDEX)

#define MEM_NORMAL_URW_ATTRS                                                                       \
    ENTRY_ATTRS(XN | PXN, ACCESS_FLAG | NON_GLOBAL | ISH | AP_RW | AP_ACCESS_UNPRIVILEGED,         \
                MEM_NORMAL_INDEX)
#define MEM_NORMAL_URWX_ATTRS                                                                      \
    ENTRY_ATTRS(0, ACCESS_FLAG | NON_GLOBAL | ISH | AP_RW | AP_ACCESS_UNPRIVILEGED,                \
                MEM_NORMAL_INDEX)
#define MEM_NORMAL_UROX_ATTRS                                                                      \
    ENTRY_ATTRS(0, ACCESS_FLAG | NON_GLOBAL | ISH | AP_RO | AP_ACCESS_UNPRIVILEGED,                \
                MEM_NORMAL_INDEX)

#define MEM_DEVICE_URW_ATTRS                                                                       \
    ENTRY_ATTRS(XN | PXN, ACCESS_FLAG | NON_GLOBAL | ISH | AP_RW | AP_ACCESS_UNPRIVILEGED,         \
                MEM_DEVICE_INDEX)
#define MEM_DEVICE_URO_ATTRS                                                                       \
    ENTRY_ATTRS(XN | PXN, ACCESS_FLAG | NON_GLOBAL | ISH | AP_RO | AP_ACCESS_UNPRIVILEGED,         \
                MEM_DEVICE_INDEX)

#define MEM_NORMAL_NC_URW_ATTRS                                                                    \
    ENTRY_ATTRS(XN | PXN, ACCESS_FLAG | NON_GLOBAL | ISH | AP_RW | AP_ACCESS_UNPRIVILEGED,         \
                MEM_NORMAL_NC_INDEX)
#define MEM_NORMAL_NC_URWX_ATTRS                                                                   \
    ENTRY_ATTRS(0, ACCESS_FLAG | NON_GLOBAL | ISH | AP_RW | AP_ACCESS_UNPRIVILEGED,                \
                MEM_NORMAL_NC_INDEX)
#define MEM_NORMAL_NC_UROX_ATTRS                                                                   \
    ENTRY_ATTRS(0, ACCESS_FLAG | NON_GLOBAL | ISH | AP_RO | AP_ACCESS_UNPRIVILEGED,                \
                MEM_NORMAL_NC_INDEX)

#define MEM_NORMAL_WT_URW_ATTRS                                                                    \
    ENTRY_ATTRS(XN | PXN, ACCESS_FLAG | NON_GLOBAL | ISH | AP_RW | AP_ACCESS_UNPRIVILEGED,         \
                MEM_NORMAL_WT_INDEX)
#define MEM_NORMAL_WT_URWX_ATTRS                                                                   \
    ENTRY_ATTRS(0, ACCESS_FLAG | ISH | NON_GLOBAL | AP_RW | AP_ACCESS_UNPRIVILEGED,                \
                MEM_NORMAL_WT_INDEX)
#define MEM_NORMAL_WT_UROX_ATTRS                                                                   \
    ENTRY_ATTRS(0, ACCESS_FLAG | ISH | NON_GLOBAL | AP_RO | AP_ACCESS_UNPRIVILEGED,                \
                MEM_NORMAL_WT_INDEX)

/*
 * MAIR encodings for device memory attributes.
 */
#define MAIR_DEV_nGnRnE U(0x0)
#define MAIR_DEV_nGnRE U(0x4)
#define MAIR_DEV_nGRE U(0x8)
#define MAIR_DEV_GRE U(0xc)

/*
 * MAIR encodings for normal memory attributes.
 *
 * Cache Policy
 *  WT:	 Write Through
 *  WB:	 Write Back
 *  NC:	 Non-Cacheable
 *
 * Transient Hint
 *  NTR: Non-Transient
 *  TR:	 Transient
 *
 * Allocation Policy
 *  RA:	 Read Allocate
 *  WA:	 Write Allocate
 *  RWA: Read and Write Allocate
 *  NA:	 No Allocation
 */
#define MAIR_NORM_WT_TR_WA U(0x1)
#define MAIR_NORM_WT_TR_RA U(0x2)
#define MAIR_NORM_WT_TR_RWA U(0x3)
#define MAIR_NORM_NC U(0x4)
#define MAIR_NORM_WB_TR_WA U(0x5)
#define MAIR_NORM_WB_TR_RA U(0x6)
#define MAIR_NORM_WB_TR_RWA U(0x7)
#define MAIR_NORM_WT_NTR_NA U(0x8)
#define MAIR_NORM_WT_NTR_WA U(0x9)
#define MAIR_NORM_WT_NTR_RA U(0xa)
#define MAIR_NORM_WT_NTR_RWA U(0xb)
#define MAIR_NORM_WB_NTR_NA U(0xc)
#define MAIR_NORM_WB_NTR_WA U(0xd)
#define MAIR_NORM_WB_NTR_RA U(0xe)
#define MAIR_NORM_WB_NTR_RWA U(0xf)

#define MAIR_NORM_OUTER_SHIFT U(4)

#define MAKE_MAIR_NORMAL_MEMORY(inner, outer) ((inner) | ((outer) << MAIR_NORM_OUTER_SHIFT))

#ifdef __aarch64__
/*
 * TCR defintions
 */
#define TCR_EL3_RES1 ((ULL(1) << 31) | (ULL(1) << 23))
#define TCR_EL2_RES1 ((ULL(1) << 31) | (ULL(1) << 23))
#define TCR_EL1_IPS_SHIFT U(32)
#define TCR_EL2_PS_SHIFT U(16)
#define TCR_EL3_PS_SHIFT U(16)

#define TCR_TxSZ_MIN ULL(16)
#define TCR_TxSZ_MAX ULL(39)
#define TCR_TxSZ_MAX_TTST ULL(48)

#define TCR_T0SZ_SHIFT U(0)
#define TCR_T1SZ_SHIFT U(16)

/* (internal) physical address size bits in EL3/EL1 */
#define TCR_PS_BITS_4GB ULL(0x0)
#define TCR_PS_BITS_64GB ULL(0x1)
#define TCR_PS_BITS_1TB ULL(0x2)
#define TCR_PS_BITS_4TB ULL(0x3)
#define TCR_PS_BITS_16TB ULL(0x4)
#define TCR_PS_BITS_256TB ULL(0x5)

#define ADDR_MASK_48_TO_63 ULL(0xFFFF000000000000)
#define ADDR_MASK_44_TO_47 ULL(0x0000F00000000000)
#define ADDR_MASK_42_TO_43 ULL(0x00000C0000000000)
#define ADDR_MASK_40_TO_41 ULL(0x0000030000000000)
#define ADDR_MASK_36_TO_39 ULL(0x000000F000000000)
#define ADDR_MASK_32_TO_35 ULL(0x0000000F00000000)

#define TCR_RGN_INNER_NC (ULL(0x0) << 8)
#define TCR_RGN_INNER_WBA (ULL(0x1) << 8)
#define TCR_RGN_INNER_WT (ULL(0x2) << 8)
#define TCR_RGN_INNER_WBNA (ULL(0x3) << 8)

#define TCR_RGN_OUTER_NC (ULL(0x0) << 10)
#define TCR_RGN_OUTER_WBA (ULL(0x1) << 10)
#define TCR_RGN_OUTER_WT (ULL(0x2) << 10)
#define TCR_RGN_OUTER_WBNA (ULL(0x3) << 10)

#define TCR_SH_NON_SHAREABLE (ULL(0x0) << 12)
#define TCR_SH_OUTER_SHAREABLE (ULL(0x2) << 12)
#define TCR_SH_INNER_SHAREABLE (ULL(0x3) << 12)

#define TCR_RGN1_INNER_NC (ULL(0x0) << 24)
#define TCR_RGN1_INNER_WBA (ULL(0x1) << 24)
#define TCR_RGN1_INNER_WT (ULL(0x2) << 24)
#define TCR_RGN1_INNER_WBNA (ULL(0x3) << 24)

#define TCR_RGN1_OUTER_NC (ULL(0x0) << 26)
#define TCR_RGN1_OUTER_WBA (ULL(0x1) << 26)
#define TCR_RGN1_OUTER_WT (ULL(0x2) << 26)
#define TCR_RGN1_OUTER_WBNA (ULL(0x3) << 26)

#define TCR_SH1_NON_SHAREABLE (ULL(0x0) << 28)
#define TCR_SH1_OUTER_SHAREABLE (ULL(0x2) << 28)
#define TCR_SH1_INNER_SHAREABLE (ULL(0x3) << 28)

#define TCR_TG0_SHIFT U(14)
#define TCR_TG0_MASK ULL(3)
#define TCR_TG0_4K (ULL(0) << TCR_TG0_SHIFT)
#define TCR_TG0_64K (ULL(1) << TCR_TG0_SHIFT)
#define TCR_TG0_16K (ULL(2) << TCR_TG0_SHIFT)

#define TCR_TG1_SHIFT U(30)
#define TCR_TG1_MASK ULL(3)
#define TCR_TG1_16K (ULL(1) << TCR_TG1_SHIFT)
#define TCR_TG1_4K (ULL(2) << TCR_TG1_SHIFT)
#define TCR_TG1_64K (ULL(3) << TCR_TG1_SHIFT)

#define TCR_EPD0_BIT (ULL(1) << 7)
#define TCR_EPD1_BIT (ULL(1) << 23)

#define TCR_A1_TTBR0_ASID (ULL(0) << 22)
#define TCR_A1_TTBR1_ASID (ULL(1) << 22)

#define TCR_ASID_8BIT (ULL(0) << 36)
#define TCR_ASID_16BIT (ULL(1) << 36)

#define TCR_TBI0 (ULL(1) << 37)
#define TCR_TBI1 (ULL(1) << 38)
#else
/*
 * TTBCR definitions
 */
#define TTBCR_EAE_BIT (U(1) << 31)
#define TTBCR_T2E_BIT (U(1) << 6)

#define TTBCR_SH1_NON_SHAREABLE (U(0x0) << 28)
#define TTBCR_SH1_OUTER_SHAREABLE (U(0x2) << 28)
#define TTBCR_SH1_INNER_SHAREABLE (U(0x3) << 28)

#define TTBCR_RGN1_OUTER_NC (U(0x0) << 26)
#define TTBCR_RGN1_OUTER_WBA (U(0x1) << 26)
#define TTBCR_RGN1_OUTER_WT (U(0x2) << 26)
#define TTBCR_RGN1_OUTER_WBNA (U(0x3) << 26)

#define TTBCR_RGN1_INNER_NC (U(0x0) << 24)
#define TTBCR_RGN1_INNER_WBA (U(0x1) << 24)
#define TTBCR_RGN1_INNER_WT (U(0x2) << 24)
#define TTBCR_RGN1_INNER_WBNA (U(0x3) << 24)

#define TTBCR_EPD1_BIT (U(1) << 23)
#define TTBCR_A1_BIT (U(1) << 22)

#define TTBCR_T1SZ_SHIFT U(16)
#define TTBCR_T1SZ_MASK U(0x7)
#define TTBCR_TxSZ_MIN U(0)
#define TTBCR_TxSZ_MAX U(7)

#define TTBCR_SH0_NON_SHAREABLE (U(0x0) << 12)
#define TTBCR_SH0_OUTER_SHAREABLE (U(0x2) << 12)
#define TTBCR_SH0_INNER_SHAREABLE (U(0x3) << 12)

#define TTBCR_RGN0_OUTER_NC (U(0x0) << 10)
#define TTBCR_RGN0_OUTER_WBA (U(0x1) << 10)
#define TTBCR_RGN0_OUTER_WT (U(0x2) << 10)
#define TTBCR_RGN0_OUTER_WBNA (U(0x3) << 10)

#define TTBCR_RGN0_INNER_NC (U(0x0) << 8)
#define TTBCR_RGN0_INNER_WBA (U(0x1) << 8)
#define TTBCR_RGN0_INNER_WT (U(0x2) << 8)
#define TTBCR_RGN0_INNER_WBNA (U(0x3) << 8)

#define TTBCR_EPD0_BIT (U(1) << 7)
#define TTBCR_T0SZ_SHIFT U(0)
#define TTBCR_T0SZ_MASK U(0x7)
#endif

#define ATTR_DEVICE_nGnRE MAIR_DEV_nGnRE
#define ATTR_IWBWA_OWBWA_NTR MAKE_MAIR_NORMAL_MEMORY(MAIR_NORM_WB_NTR_RWA, MAIR_NORM_WB_NTR_RWA)
#define ATTR_NON_CACHEABLE MAKE_MAIR_NORMAL_MEMORY(MAIR_NORM_NC, MAIR_NORM_NC)
#define ATTR_IWTWA_OWTWA_NTR MAKE_MAIR_NORMAL_MEMORY(MAIR_NORM_WT_NTR_RWA, MAIR_NORM_WT_NTR_RWA)

#define MAIR_ATTR_SET(attr, index) ((attr) << ((index) << 3))
#define ATTR_INDEX_MASK U(0x3)
#define ATTR_INDEX_GET(attr) (((attr) >> 2) & ATTR_INDEX_MASK)

#ifdef __aarch64__
/* Guarded Page bit */
#define GP (ULL(1) << 50)
#endif

#define ENTRY_TYPE_MASK 3UL
#define ENTRY_TYPE_USED 1UL
#define ENTRY_TYPE_BLOCK 1UL
#define ENTRY_TYPE_TABLE 3UL
#define ENTRY_TYPE_PAGE 3UL

#define ENTRY_ADDRESS_MASK ULL(0x0000fffffffff000)
#define ENTRY_ATTRIB_MASK ULL(0xffff000000000ffc)

#define ENTRY_ATTRIB_UP_MASK UL(0xffff000000000000)
#define ENTRY_ATTRIB_DOWN_MASK UL(0x0000000000000ffc)

#define TABLE_LEVEL_MASK UL(0x1ff)

#define TABLE_ENTRIES (PAGE_SIZE / sizeof(uint64_t))

#define PAGE_4K_SIZE_SHIFT (12)
#define _512_SIZE_SHIFT (9)
#define MAX_TABLE_LEVEL (3)
#ifdef __aarch64__
#define MIN_MAP_LEVEL (1)
#else
#define MIN_MAP_LEVEL (1)
#endif

#define LEVEL_FINAL_SIZE_SHIFT (PAGE_4K_SIZE_SHIFT)
#define LEVEL_SIZE_SHIFT(level)                                                                    \
    (LEVEL_FINAL_SIZE_SHIFT + (MAX_TABLE_LEVEL - (level)) * _512_SIZE_SHIFT) // 2 21
#define LEVEL_SIZE(level) (UL(0x01) << LEVEL_SIZE_SHIFT(level))
#define LEVEL_SIZE_MASK(level) (LEVEL_SIZE(level) - 1)
#define LEVEL_SIZE_ALIGN(level, addr) ((addr) & (~LEVEL_SIZE_MASK(level)))

#define IS_LEVEL_ALIGN(level, addr) (((addr)&LEVEL_SIZE_MASK(level)) == U(0))
#define GET_TABLE_OFF(level, addr) (((addr) >> LEVEL_SIZE_SHIFT(level)) & TABLE_LEVEL_MASK)

#define PAGE_SIZE LEVEL_SIZE(MAX_TABLE_LEVEL)
#define PAGE_SIZE_SHIFT LEVEL_SIZE_SHIFT(MAX_TABLE_LEVEL)
#define PAGE_SIZE_MASK LEVEL_SIZE_MASK(MAX_TABLE_LEVEL)
#define PAGE_SIZE_ALIGN(addr) LEVEL_SIZE_ALIGN(MAX_TABLE_LEVEL, addr)

    static inline int GET_MAP_LEVEL(uintptr_t va, uint64_t pa, size_t size)
{
    int level;
    for (level = MIN_MAP_LEVEL; level < MAX_TABLE_LEVEL; level++)
    {
        if (IS_LEVEL_ALIGN(level, va) && IS_LEVEL_ALIGN(level, pa) && IS_LEVEL_ALIGN(level, size))
        {
            return level;
        }
    }
    return level;
}

#define TABLE_DESC_EX(attr, next_table_point)                                                      \
    (((attr) << 59) | ((next_table_point)&0x0000FFFFFFFFF000ULL) | ENTRY_TYPE_TABLE)

#define TABLE_DESC(next_table_point) TABLE_DESC_EX(0ULL, ((uint64_t)next_table_point))

#define BLOCK_DESC(attr, addr)                                                                     \
    (((attr)&ENTRY_ATTRIB_MASK) | (((uint64_t)addr) & ENTRY_ADDRESS_MASK) | ENTRY_TYPE_BLOCK)

#define PAGE_DESC_4K(attr, addr)                                                                   \
    (((attr)&ENTRY_ATTRIB_MASK) | (((uint64_t)addr) & ENTRY_ADDRESS_MASK) | ENTRY_TYPE_PAGE)

#define PAGE_DESC PAGE_DESC_4K

void mmu_mair_tcr_init(void);

#endif
