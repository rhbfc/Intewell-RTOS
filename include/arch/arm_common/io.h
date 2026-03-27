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

/*
 * @file： io.h
 * @brief：
 *	    <li>IO操作相关接口。</li>
 */
#ifndef _IO_H
#define _IO_H

/************************头文件********************************/
#include <barrier.h>
#include <errno.h>
#include <stdbool.h>
#include <system/types.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/************************宏定义********************************/

/* 体系结构通用IO读写接口 */
#define arch_readb(a) (*(volatile unsigned char *)(a))
#define arch_readw(a) (*(volatile unsigned short *)(a))
#define arch_readl(a) (*(volatile unsigned int *)(a))
#define arch_readq(a) (*(volatile unsigned long long *)(a))

#define arch_writeb(v, a) (*(volatile unsigned char *)(a) = (v))
#define arch_writew(v, a) (*(volatile unsigned short *)(a) = (v))
#define arch_writel(v, a) (*(volatile unsigned int *)(a) = (v))
#define arch_writeq(v, a) (*(volatile unsigned long long *)(a) = (v))

/* 编译栅栏 */
#define barrier() __asm__ __volatile__("" : : : "memory")

/* IO栅栏 */
#define iormb() rmb()
#define iowmb() wmb()

/* 通用IO读写接口(暂未考虑大小端) */
#define readb(c)                                                                                   \
    ({                                                                                             \
        unsigned char v = arch_readb(c);                                                           \
        iormb();                                                                                   \
        v;                                                                                         \
    })
#define readw(c)                                                                                   \
    ({                                                                                             \
        unsigned short v = arch_readw(c);                                                          \
        iormb();                                                                                   \
        v;                                                                                         \
    })
#define readl(c)                                                                                   \
    ({                                                                                             \
        unsigned int v = arch_readl(c);                                                            \
        iormb();                                                                                   \
        v;                                                                                         \
    })
#define readq(c)                                                                                   \
    ({                                                                                             \
        unsigned long long v = arch_readq(c);                                                      \
        iormb();                                                                                   \
        v;                                                                                         \
    })

#define writeb(v, c)                                                                               \
    ({                                                                                             \
        iowmb();                                                                                   \
        arch_writeb((v), (c));                                                                     \
    })
#define writew(v, c)                                                                               \
    ({                                                                                             \
        iowmb();                                                                                   \
        arch_writew((v), (c));                                                                     \
    })
#define writel(v, c)                                                                               \
    ({                                                                                             \
        iowmb();                                                                                   \
        arch_writel((v), (c));                                                                     \
    })
#define writeq(v, c)                                                                               \
    ({                                                                                             \
        iowmb();                                                                                   \
        arch_writeq((v), (c));                                                                     \
    })

#define _swab32(x)                                                                                 \
    ((u32)((((u32)(x) & (u32)0x000000ffUL) << 24) | (((u32)(x) & (u32)0x0000ff00UL) << 8) |        \
           (((u32)(x) & (u32)0x00ff0000UL) >> 8) | (((u32)(x) & (u32)0xff000000UL) >> 24)))

#define _swab16(x) ((u16)((((u16)(x) & (u16)0x00ffUL) << 8) | (((u16)(x) & (u16)0xff00UL) << 8)))

#define _swab64(x) ((u64)_swab32((u64)(x)&0xFFFFFFFFUL) << 32 | ((u64)_swab32((u64)(x) >> 32)))

#define cpu_to_be64(x) _swab64(x)
#define be64_to_cpu(x) _swab64(x)

#define cpu_to_be32(x) _swab32(x)
#define be32_to_cpu(x) _swab32(x)

#define cpu_to_be16(x) _swab16(x)
#define be16_to_cpu(x) _swab16(x)

#define in_arch(type, endian, a) endian##_to_cpu(read##type(a))
#define out_arch(type, endian, a, v) write##type(cpu_to_##endian(v), a)

#define out_be32(a, v) out_arch(l, be32, a, v)
#define in_be32(a) in_arch(l, be32, a)

#define out_be16(a, v) out_arch(l, be16, a, v)
#define in_be16(a) in_arch(l, be16, a)

#define out_le32(a, v) writel(v, a)
#define in_le32(a) readl(a)

#define out_le16(a, v) writew(v, a)
#define in_le16(a) readw(a)

#define out_8(a, v) writeb(v, a)
#define in_8(a) readb(a)

#define clrbits(type, addr, clear) out_##type((addr), in_##type(addr) & ~(clear))

#define setbits(type, addr, set) out_##type((addr), in_##type(addr) | (set))

#define clrsetbits(type, addr, clear, set) out_##type((addr), (in_##type(addr) & ~(clear)) | (set))

#define clrbits_le32(addr, clear) clrbits(le32, addr, clear)
#define setbits_le32(addr, set) setbits(le32, addr, set)
#define clrsetbits_le32(addr, clear, set) clrsetbits(le32, addr, clear, set)

#define clrbits_le16(addr, clear) clrbits(le16, addr, clear)
#define setbits_le16(addr, set) setbits(le16, addr, set)
#define clrsetbits_le16(addr, clear, set) clrsetbits(le16, addr, clear, set)

#define clrbits_be32(addr, clear) clrbits(be32, addr, clear)
#define setbits_be32(addr, set) setbits(be32, addr, set)
#define clrsetbits_be32(addr, clear, set) clrsetbits(be32, addr, clear, set)

#define clrbits_8(addr, clear) clrbits(8, addr, clear)
#define setbits_8(addr, set) setbits(8, addr, set)
#define clrsetbits_8(addr, clear, set) clrsetbits(8, addr, clear, set)

#define div_u64(a, b) (a / b)

/************************类型定义******************************/

int usleep(unsigned);

/************************接口声明******************************/

static inline int wait_for_bit_le32(const void *reg, const u32 mask, const bool set,
                                    const unsigned int timeout_ms, const bool breakable)
{
    u32 val;
    u32 time = 0;
    u64 timeout_us = timeout_ms * 1000000;

    while (1)
    {
        val = readl(reg);

        if (!set)
            val = ~val;

        if ((val & mask) == mask)
            return 0;

        if (time > timeout_us)
        {
            break;
        }
        usleep(1);
        time++;
    }
    return -ETIMEDOUT;
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _IO_H */
