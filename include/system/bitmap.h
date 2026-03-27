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
 * @file： bitmap.h
 * @brief：
 *    <li>bitmap功能实现</li>
 */

#ifndef _BITMAP_H
#define _BITMAP_H

/************************头文件********************************/
#include <system/bitops.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/************************宏定义********************************/
#define BITMAP_LAST_WORD_MASK(nbits)                                           \
    (((nbits) % BITS_PER_LONG) ? (1UL << ((nbits) % BITS_PER_LONG)) - 1 : ~0UL)

#define DECLARE_BITMAP(name, nbits) unsigned long name[BITS_TO_LONGS (nbits)]

/************************类型定义******************************/

/**
 * @brief
 *    判断位图是否为满
 * @param[in] bitmap 位图指针
 * @param[in] bits 要判断位图的位数
 * @retval 0 位图不满
 * @retval 1 位图满
 */
static inline int __bitmap_full (const unsigned long *bitmap, int bits)
{
    int k, lim = bits / BITS_PER_LONG;

    for (k = 0; k < lim; ++k)
    {
        if (~bitmap[k])
        {
            return 0;
        }
    }

    if (bits % BITS_PER_LONG)
    {
        if (~bitmap[k] & BITMAP_LAST_WORD_MASK (bits))
        {
            return 0;
        }
    }

    return 1;
}

/**
 * @brief
 *    判断位图是否为空
 * @param[in] bitmap 位图指针
 * @param[in] bits 要判断位图的位数
 * @retval 0 位图不为空
 * @retval 1 位图为空
 */
static inline int __bitmap_empty (const unsigned long *bitmap, int bits)
{
    int k, lim = bits / BITS_PER_LONG;

    for (k = 0; k < lim; ++k)
    {
        if (bitmap[k])
        {
            return 0;
        }
    }

    if (bits % BITS_PER_LONG)
    {
        if (bitmap[k] & BITMAP_LAST_WORD_MASK (bits))
        {
            return 0;
        }
    }

    return 1;
}

/**
 * @brief
 *    设置位图指定位
 * @param[in] bmap 位图指针
 * @param[in] bit 要设置的位
 * @retval 无
 */
static inline void bitmap_setbit (unsigned long *bmap, unsigned long bit)
{
    bmap[BIT_WORD (bit)] |= (0x1UL << BIT_WORD_OFFSET (bit));
}

/**
 * @brief
 *    清0位图指定位
 * @param[in] bmap 位图指针
 * @param[in] bit 要清0的位
 * @retval 无
 */
static inline void bitmap_clearbit (unsigned long *bmap, unsigned long bit)
{
    bmap[BIT_WORD (bit)] &= ~(0x1UL << BIT_WORD_OFFSET (bit));
}

/**
 * @brief
 *    从起始位设置连续nbits为1
 * @param[in] bmap 位图指针
 * @param[in] start 起始位
 * @param[in] len 位数
 * @retval 无
 */
static inline void bitmap_set (unsigned long *bmap, int start, int nbits)
{
    int bit;

    for (bit = start; bit < (start + nbits); bit++)
    {
        bmap[BIT_WORD (bit)] |= (0x1UL << BIT_WORD_OFFSET (bit));
    }
}

/**
 * @brief
 *    从指定起始位清0指定长度的位
 * @param[in] bmap 位图指针
 * @param[in] start 起始位
 * @param[in] nbits 位数
 * @retval 无
 */
static inline void bitmap_clear (unsigned long *bmap, int start, int nbits)
{
    int bit;

    for (bit = start; bit < (start + nbits); bit++)
    {
        bmap[BIT_WORD (bit)] &= ~(0x1UL << BIT_WORD_OFFSET (bit));
    }
}

/**
 * @brief
 *    从起始位置清0位图指定位数
 * @param[in] bmap 位图指针
 * @param[in] nbits 要清0的位数
 * @retval 无
 */
static inline void bitmap_zero (unsigned long *bmap, int nbits)
{
    if (small_const_nbits (nbits))
    {
        *bmap = 0UL;
    }
    else
    {
        int len = BITS_TO_LONGS (nbits) * sizeof (unsigned long);
        memset (bmap, 0, len);
    }
}

/**
 * @brief
 *    从起始位置设置位图指定位数
 * @param[in] bmap 位图指针
 * @param[in] nbits 要设置的位数
 * @retval 无
 */
static inline void bitmap_fill (unsigned long *bmap, int nbits)
{
    size_t nlongs = BITS_TO_LONGS (nbits);

    if (!small_const_nbits (nbits))
    {
        int len = (nlongs - 1) * sizeof (unsigned long);
        memset (bmap, 0xff, len);
    }

    bmap[nlongs - 1] = BITMAP_LAST_WORD_MASK (nbits);
}

/**
 * @brief
 *    判断位图的前n位是否全为0
 * @param[in] bmap 位图指针
 * @param[in] nbits 位数
 * @retval TRUE 位图的前n位全为0
 * @retval FALSE 位图的前n位不全为0
 */
static inline int bitmap_empty (const unsigned long *bmap, int nbits)
{
    if (small_const_nbits (nbits))
    {
        return !(*bmap & BITMAP_LAST_WORD_MASK (nbits));
    }
    else
    {
        return __bitmap_empty (bmap, nbits);
    }
}

/**
 * @brief
 *    判断位图的前n位是否全为1
 * @param[in] bmap 位图指针
 * @param[in] nbits 位数
 * @retval TRUE 位图的前n位全为1
 * @retval FALSE 位图的前n位不全为1
 */
static inline int bitmap_full (const unsigned long *src, int nbits)
{
    if (small_const_nbits (nbits))
    {
        return !(~(*src) & BITMAP_LAST_WORD_MASK (nbits));
    }
    else
    {
        return __bitmap_full (src, nbits);
    }
}

/**
 * @brief
 *    分配一个有n个位的位图
 * @param[in] nbits 位图的位数
 * @retval NULL 分配失败
 * @retval 非NULL 位图的起始地址
 */
static inline unsigned long *bitmap_alloc (unsigned int nbits)
{
    return memalign (4, BITS_TO_LONGS (nbits) * sizeof (unsigned long));
}

/**
 * @brief
 *    分配一个有n个位的位图,并清0
 * @param[in] nbits 位图的位数
 * @retval NULL 分配失败
 * @retval 非NULL 位图的起始地址
 */
static inline unsigned long *bitmap_zalloc (unsigned int nbits)
{
    unsigned long *bitmap = bitmap_alloc (nbits);

    if (bitmap)
    {
        memset (bitmap, 0, BITS_TO_LONGS (nbits) * sizeof (unsigned long));
    }

    return bitmap;
}

/**
 * @brief
 *    释放位图空间
 * @param[in] bmap 位图指针
 * @retval 无
 */
static inline void bitmap_free (const unsigned long *bitmap)
{
    free ((void *)bitmap);
}

/************************外部声明******************************/

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _BITMAP_H */
