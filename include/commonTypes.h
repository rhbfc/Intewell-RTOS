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
 * @file： commonTypes.h
 * @brief：
 *      <li>提供系统类型和普通宏的定义。</li>
 */

#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

/************************头文件********************************/
#include <system/const.h>
#include <system/types.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/************************宏定义********************************/

#if !defined(TRUE) || (TRUE != 1)
#undef TRUE
#define TRUE 1U
#endif

#if !defined(FALSE) || (FALSE != 0)
#undef FALSE
#define FALSE 0U
#endif

#ifndef __cplusplus

#else /* __cplusplus */
#define _Bool bool
#if __cplusplus < 201103L
#define bool bool
#define false false
#define true true
#endif
#endif /* __cplusplus */

#define big           0xffffffffU
#define little        0x12345678U
#define T_VOID        void            /* void类型 */
#define T_VVOID       volatile void   /* 非优化的void类型 */
#define T_MODULE      static          /* static修饰符 */
#define T_INLINE      T_MODULE inline /* static inline 修饰符 */
#define T_EXTERN      extern          /* extern修饰符 */
#define T_VOLATILE    volatile        /* volatile修饰符 */
#define T_CONST       const           /* const修饰符 */
#define T_UWORD_MAX   0xffffffffU
#define UNUSED_ARG(x) (void)x

/************************类型定义******************************/

#ifndef ASM_USE

/* 类型定义 */

typedef char T_BYTE; /* 有符号8bit; arm 32位模式下char默认为unsigned char，*/
typedef unsigned char T_UBYTE; /* 无符号8bit*/
typedef char          T_CHAR;  /* 有符号8bit*/

typedef signed short   T_HWORD;  /* 有符号16bit*/
typedef unsigned short T_UHWORD; /* 无符号16bit*/

typedef signed int   T_WORD;  /* 有符号32bit*/
typedef unsigned int T_UWORD; /* 无符号32bit*/

typedef double T_DOUBLE; /* 双精度浮点型*/
typedef float  T_FLOAT;  /* 单精度浮点型*/

typedef unsigned int T_BOOL; /* 无符号32bit*/

typedef volatile signed char   T_VBYTE;  /* 非优化的有符号8bit*/
typedef volatile unsigned char T_VUBYTE; /* 非优化的无符号8bit*/

typedef volatile signed short   T_VHWORD;  /* 非优化的有符号16bit*/
typedef volatile unsigned short T_VUHWORD; /* 非优化的无符号16bit*/

typedef volatile signed int   T_VWORD;  /* 非优化的有符号32bit*/
typedef volatile unsigned int T_VUWORD; /* 非优化的无符号32bit*/

typedef volatile double T_VDOUBLE; /* 非优化的双精度浮点型*/
typedef volatile float  T_VFLOAT;  /* 非优化的单精度浮点型*/

typedef volatile unsigned int T_VBOOL; /* 非优化的无符号32bit*/

typedef unsigned char  UINT8;  /* 无符号8bit */
typedef unsigned short UINT16; /* 无符号16bit */
typedef unsigned int   UINT32; /* 无符号32bit */
typedef char           INT8;   /* 有符号8bit */
typedef signed short   INT16;  /* 有符号16bit */
typedef signed int     INT32;  /* 有符号32bit */
typedef float          SINGLE; /* 单精度浮点型 */
typedef double         DOUBLE; /* 双精度浮点型 */
typedef unsigned int   BOOL;   /* 无符号32bit */

typedef long          T_LONG;
typedef unsigned long T_ULONG;

/* 对象名字类型 */
typedef UINT8 *Os_Name;

/* 对象ID类型 */
typedef UINT32 Os_Id;

typedef unsigned long ADDRESS;

typedef unsigned long u_register_t;

#ifdef CONFIG_OS_LP64
typedef signed long            T_DWORD;   /* 有符号64bit*/
typedef unsigned long          T_UDWORD;  /* 无符号64bit*/
typedef volatile signed long   T_VDWORD;  /* 非优化的有符号64bit*/
typedef volatile unsigned long T_VUDWORD; /* 非优化的无符号64bit*/
typedef unsigned long          UINT64;    /* 无符号64bit */
typedef signed long            INT64;     /* 有符号64bit */
#else
typedef signed long long            T_DWORD;   /* 有符号64bit*/
typedef unsigned long long          T_UDWORD;  /* 无符号64bit*/
typedef volatile signed long long   T_VDWORD;  /* 非优化的有符号64bit*/
typedef volatile unsigned long long T_VUDWORD; /* 非优化的无符号64bit*/
typedef unsigned long long          UINT64;    /* 无符号64bit */
typedef signed long long            INT64;     /* 有符号64bit */
#endif

#endif /* ASM_USE */
/************************接口声明******************************/

#ifdef __cplusplus
}
#endif /*__cplusplus */

#endif /*COMMON_TYPES_H*/
