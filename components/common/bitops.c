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
 * @file： bitops.c
 * @brief：
 *    <li>位操作相关函数和宏定义</li>
 */

/************************头 文 件******************************/
#include <system/bitops.h>

/************************宏 定 义******************************/

/************************类型定义******************************/

/************************外部声明******************************/

/************************前向声明******************************/

/************************模块变量******************************/

/************************全局变量******************************/

/************************函数实现******************************/

/**
 * @brief
 *    查找第一个为0的位
 * @param[in] addr 位图指针
 * @param[in] bits 要查找位图的位数
 * @retval 非bits 第一个被清0的位的索引
 * @retval bits 不存在被清0的位
 */
unsigned long find_first_zero_bit (const unsigned long *addr,
                                   unsigned long        bits)
{
    if (small_const_nbits (bits))
    {
        unsigned long val = *addr | ~GENMASK (bits - 1, 0);

        return val == ~0UL ? bits : ffz (val);
    }

    return _find_first_zero_bit (addr, bits);
}

/**
 * @brief
 *    查找第一个为1的位
 * @param[in] addr 地址空间起始
 * @param[in] bits 要查找的位数
 * @retval 非bits 第一个为1的位的索引
 * @retval bits 不存在为1的位
 */
unsigned long find_first_bit (const unsigned long *addr, unsigned long bits)
{
    if (small_const_nbits (bits))
    {
        unsigned long val = *addr & GENMASK (bits - 1, 0);

        return val ? __ffs (val) : bits;
    }

    return _find_first_bit (addr, bits);
}

/**
 * @brief
 *    查找下一个为1的位
 * @param[in] addr 地址空间起始
 * @param[in] bits 总的位数
 * @param[in] start_bit 起始位
 * @retval 非bits 第一个为1的位的索引
 * @retval bits 下一个不存在为1的位
 */
unsigned long find_next_bit (const unsigned long *addr, unsigned long bits,
                             unsigned long start_bit)
{
    if (small_const_nbits (bits))
    {
        unsigned long val;

        if (unlikely (start_bit >= bits))
        {
            return bits;
        }

        val = *addr & GENMASK (bits - 1, start_bit);
        return val ? __ffs (val) : bits;
    }

    return _find_next_bit (addr, bits, start_bit);
}
