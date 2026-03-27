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
 * @file： types.h
 * @brief：
 *      <li>提供系统类型和普通宏的定义。</li>
 */
#ifndef _TYPES_H
#define _TYPES_H

/************************头文件********************************/
#include <system/const.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/************************宏定义********************************/

/************************类型定义******************************/

#ifndef ASM_USE

#include <stddef.h>

typedef signed char   s8; /* 有符号8bit */
typedef unsigned char u8; /* 无符号8bit */

typedef signed short   s16; /* 有符号16bit */
typedef unsigned short u16; /* 无符号16bit */

typedef signed int   s32; /* 有符号32bit */
typedef unsigned int u32; /* 无符号32bit */

#ifndef CONFIG_OS_LP64
typedef long long          s64; /* 有符号64bit */
typedef unsigned long long u64; /* 无符号64bit */
#else
typedef long          s64; /* 有符号64bit */
typedef unsigned long u64; /* 无符号64bit */
#endif

/* 地址类型定义，物理地址统一定为64位，32位系统也支持物理地址扩展为64位地址
 */
typedef u64           phys_addr_t; /* 物理地址 */
typedef phys_addr_t   dma_addr_t;  /* DMA地址 */
typedef unsigned long virt_addr_t; /* 虚拟地址 */
typedef u64           ipa_addr_t;  /* 虚拟机物理地址 */
typedef unsigned long long pfn_t;  /* pfn */

typedef __signed__ char __s8;
typedef unsigned char 	__u8;
typedef __signed__ short __s16;
typedef unsigned short __u16;
typedef __signed__ int __s32;
typedef unsigned int __u32;
typedef __signed__ long __s64;
typedef unsigned long __u64;
typedef u16 __le16;
typedef u16 __be16;
typedef u32 __le32;
typedef u32 __be32;
typedef u64 __le64;
typedef u64 __be64;

#endif /* ASM_USE */

/************************接口声明******************************/
#ifdef __cplusplus
}

#endif /*__cplusplus */

#endif /*_TYPES_H */
