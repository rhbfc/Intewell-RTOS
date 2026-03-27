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
 * @file： bitops.h
 * @brief：
 *    <li>位操作功能</li>
 */

#ifndef _BITOPS_H
#define _BITOPS_H

/************************头文件********************************/
#include <system/macros.h>
#include <bitsperlong.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/************************宏定义********************************/
#define BITS_PER_BYTE        8
#define BITS_TO_LONGS(nbits) (((nbits) + BITS_PER_LONG - 1) / BITS_PER_LONG)
#define BIT_MASK(nr)         (1UL << ((nr) % BITS_PER_LONG))
#define BIT_WORD(bit)        ((bit) / BITS_PER_LONG)
#define BIT_WORD_OFFSET(bit) ((bit) & (BITS_PER_LONG - 1))

#define BITS_PER_LONG_LONG (64)

#define BIT_ULL(nr)      (1ULL << (nr))
#define BIT_ULL_MASK(nr) (1ULL << ((nr) % BITS_PER_LONG_LONG))
#define BIT_ULL_WORD(nr) ((nr) / BITS_PER_LONG_LONG)

#define GENMASK(h, l)                                                          \
    (((~0UL) - (1UL << (l)) + 1) & (~0UL >> (BITS_PER_LONG - 1 - (h))))

#define GENMASK_ULL(h, l)                                                      \
    (((~0ULL) - (1ULL << (l)) + 1) & (~0ULL >> (BITS_PER_LONG_LONG - 1 - (h))))

#define BITOP_WORD(nr) ((nr) / BITS_PER_LONG)

#define small_const_nbits(nbits)                                               \
    (__builtin_constant_p (nbits) && (nbits) <= BITS_PER_LONG)

#define BITMAP_FIRST_WORD_MASK(start) (~0UL << ((start) % BITS_PER_LONG))

#define ffz(x) __ffs (~(x))

#define FIND_FIRST_BIT(FETCH, MUNGE, size)                                     \
    ({                                                                         \
        unsigned long idx, val, sz = (size);                                   \
                                                                               \
        for (idx = 0; idx * BITS_PER_LONG < sz; idx++)                         \
        {                                                                      \
            val = (FETCH);                                                     \
            if (val)                                                           \
            {                                                                  \
                sz = min (idx * BITS_PER_LONG + __ffs (MUNGE (val)), sz);      \
                break;                                                         \
            }                                                                  \
        }                                                                      \
                                                                               \
        sz;                                                                    \
    })

#define FIND_NEXT_BIT(FETCH, MUNGE, size, start)                               \
    ({                                                                         \
        unsigned long mask, idx, tmp, sz = (size), __start = (start);          \
                                                                               \
        if (unlikely (__start >= sz))                                          \
            goto out;                                                          \
                                                                               \
        mask = MUNGE (BITMAP_FIRST_WORD_MASK (__start));                       \
        idx  = __start / BITS_PER_LONG;                                        \
                                                                               \
        for (tmp = (FETCH)&mask; !tmp; tmp = (FETCH))                          \
        {                                                                      \
            if ((idx + 1) * BITS_PER_LONG >= sz)                               \
                goto out;                                                      \
            idx++;                                                             \
        }                                                                      \
                                                                               \
        sz = min (idx * BITS_PER_LONG + __ffs (MUNGE (tmp)), sz);              \
    out:                                                                       \
        sz;                                                                    \
    })

/************************类型定义******************************/
/*
 * @brief
 *    判断位图是否为满
 * @param[in] bitmap 位图指针
 * @param[in] bits 要判断位图的位数
 * @retval 从bit0开始连续0的个数(等价出现第一个1的索引)
 */
static inline unsigned long __ffs (unsigned long word)
{
    return __builtin_ctzl (word);
}

/**
 * @brief
 *    查找下一个为0的位
 * @param[in] addr 空间地址
 * @param[in] total_bits 总位数
 * @param[in] start_bit 起始位
 * @retval 无
 */
static inline unsigned long _find_next_zero_bit (const unsigned long *addr,
                                                 unsigned long total_bits,
                                                 unsigned long start_bit)
{
    return FIND_NEXT_BIT (~addr[idx], /* nop */, total_bits, start_bit);
}

/**
 * @brief
 *    查找下一个为0的位
 * @param[in] addr 空间地址
 * @param[in] total_bits 总位数
 * @param[in] start_bit 起始位
 * @retval 无
 */
static inline unsigned long find_next_zero_bit (const unsigned long *addr,
                                                unsigned long        total_bits,
                                                unsigned long        start_bit)
{
    if (small_const_nbits (total_bits))
    {
        unsigned long val;

        if (unlikely (start_bit >= total_bits))
        {
            return total_bits;
        }

        val = *addr | ~GENMASK (total_bits - 1, start_bit);
        return val == ~0UL ? total_bits : ffz (val);
    }

    return _find_next_zero_bit (addr, total_bits, start_bit);
}

/*
 * @brief
 *    查找第一个为0的位
 * @param[in] bitmap 位图指针
 * @param[in] bits 要查找位图的位数
 * @retval 0 第一个为0的位的索引
 * @retval bits 不存在为0的位
 */
static inline unsigned long _find_first_zero_bit (const unsigned long *addr,
                                                  unsigned long        bits)
{
    return FIND_FIRST_BIT (~addr[idx], /* nop */, bits);
}

/*
 * @brief
 *    查找第一个为1的位
 * @param[in] bitmap 位图指针
 * @param[in] bits 要查找位图的位数
 * @retval 0 第一个为1的位的索引
 * @retval bits 不存在为1的位
 */
static inline unsigned long _find_first_bit (const unsigned long *addr,
                                             unsigned long        bits)
{
    return FIND_FIRST_BIT (addr[idx], /* nop */, bits);
}

/*
 * @brief
 *    查找第一个为1的位
 * @param[in] bitmap 位图指针
 * @param[in] bits 要查找位图的位数
 * @retval 0 第一个为1的位的索引
 * @retval bits 不存在为1的位
 */
static inline unsigned long _find_next_bit (const unsigned long *addr,
                                            unsigned long        nbits,
                                            unsigned long        start)
{
    return FIND_NEXT_BIT (addr[idx], /* nop */, nbits, start);
}

#define for_each_set_bit(bit, addr, size)                                      \
    for ((bit) = find_first_bit ((addr), (size)); (bit) < (size);              \
         (bit) = find_next_bit ((addr), (size), (bit) + 1))

#define for_each_clear_bit(bit, addr, size)                                    \
    for ((bit) = find_first_zero_bit ((addr), (size)); (bit) < (size);         \
         (bit) = find_next_zero_bit ((addr), (size), (bit) + 1))

/************************外部声明******************************/
unsigned long find_first_zero_bit (const unsigned long *addr,
                                   unsigned long        bits);
unsigned long find_first_bit (const unsigned long *addr, unsigned long bits);
unsigned long find_next_bit (const unsigned long *addr, unsigned long bits,
                             unsigned long start_bit);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* _BITOPS_H */
