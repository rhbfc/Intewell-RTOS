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
 * @file： commonUtils.h
 * @brief：
 *      <li>提供 Utils的相关的宏定义、类型定义和接口声明。</li>
 * @implements：
 */

#ifndef COMMON_UTILS_H
#define COMMON_UTILS_H

/************************头 文 件******************************/
#include <commonTypes.h>
#include <stdarg.h>
#include <system/compiler.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/************************宏 定 义******************************/
#ifndef ASM_USE
#ifdef CONFIG_OS_LP64
#define ULONG_MAX_BIT_INDEX (63U)
#else
#define ULONG_MAX_BIT_INDEX (31U)
#endif
#define UWORD_MAX_BIT_INDEX (31U)
#define UWORD_BIT_COUNT (31U)

/* 对象名字长度，主要用于分区和时间调度表对象名 */
#define VMK_OBJECT_NAME_LENGTH 32U

#define PRINT_BUF_SIZE 256U

#ifndef COMPILER_MEMORY_BARRIER
#define COMPILER_MEMORY_BARRIER() __asm__ volatile("" ::: "memory")
#endif

#define MSB(x) (((x) >> 8) & 0xff) /* 获取2字节整型的高字节 */
#define LSB(x) ((x)&0xff)          /* 获取2字节整型的低字节 */

#define SWAP16(x) ((LSB(x) << 8) | (MSB(x)))

#define SWAP32(x)                                                                                  \
    ((((T_UWORD)(x) & (T_UWORD)0xff000000) >> 24) | (((T_UWORD)(x) & (T_UWORD)0x00ff0000) >> 8) |  \
     (((T_UWORD)(x) & (T_UWORD)0x0000ff00) << 8) | (((T_UWORD)(x) & (T_UWORD)0x000000ff) << 24))

#ifdef _BIG_ENDIAN_
#define SHORTSWAP(x) SWAP16(x)
#define LONGSWAP(x) SWAP32(x)
#else
#define SHORTSWAP(x) ((T_UHWORD)(x))
#define LONGSWAP(x) ((T_UWORD)(x))
#endif

#define READ_MEM 0
#define WRITE_MEM 1

/* 默认的空间分配对齐大小 */
#define DEFAULT_MALLOC_ALIGN_SIZE ((UINT32)8U)

/************************类型定义******************************/
/* 指针，回调函数 */
typedef T_VOID (*callbackhandler)(T_VOID);

typedef T_VOID (*T_DefaultHandler)(T_VOID);

/* API的返回值类型定义 */

typedef enum
{
    /*操作成功*/
    RET_OK = 0,

    /*操作失败*/
    RET_FAIL = 1,

    /*超时*/
    RET_TIMEOUT = 2,

    /*操作被屏蔽*/
    RET_MASKED = 3,

    /*无效的ID*/
    RET_INVALID_ID = 4,

    /*无效的名字 5*/
    RET_INVALID_NAME = 5,

    /*无效的地址*/
    RET_INVALID_ADDRESS = 6,

    /*无效的时间*/
    RET_INVALID_TIME = 7,

    /*无效的状态*/
    RET_INVALID_STATE = 8,

    /*无效的索引*/
    RET_INVALID_INDEX = 9,

    /*无效的大小 10*/
    RET_INVALID_SIZE = 10,

    /*无效的对齐 */
    RET_INVALID_ALIGNED = 11,

    /*无效的类型*/
    RET_INVALID_TYPE = 12,

    /*无效的内容*/
    RET_INVALID_CONTENT = 13,

    /*无效的用户*/
    RET_INVALID_USER = 14,

    /*在中断中处理程序中执行*/
    RET_CALLED_FROM_ISR = 15,

    /* 没有请求到资源 */
    RET_UNSATISFIED = 16,
} T_ReturnCode;

/** 空间访问类型 */
typedef enum
{
    /* 保护模式下读访问 */
    SPACE_READ = 1,

    /* 保护模式下写访问 */
    SPACE_WRITE,

    /* 实模式下读访问 */
    SPACE_FORCE_READ,

    /* 实模式下写访问 */
    SPACE_FORCE_WRITE,

    /* 更新指令Cache */
    SPACE_UPDATE_INSTRUCTION,

    /* 刷新Cache */
    SPACE_FLUSH_CACHE,

    /* 刷新并且无效cache */
    SPACE_FLUSH_INVALID_CACHE,

    /* 无效cache */
    SPACE_INVALIDATE_CACHE
} T_AccessType;

/* 对齐类型 */
typedef enum
{
    /* 单字节对齐 */
    ALIGNED_BY_ONE_BYTE = 1,

    /* 双字节对齐 */
    ALIGNED_BY_TWO_BYTE = 2,

    /* 四字节对齐 */
    ALIGNED_BY_FOUR_BYTE = 4,

} T_AlignType;

/* 时间单位 */
typedef enum
{
    /* 单位ns */
    NS_UNIT = 0,

    /* 单位us  */
    US_UNIT,

    /* 单位ms */
    MS_UNIT,

    /* 单位s  */
    S_UNIT

} T_TimeUnitType;

/************************外部声明******************************/
void memAlignCpy(T_VOID *dest, T_VOID *src, T_UWORD size, T_AlignType alignType);
void memAlignClear(T_VOID *dest, T_UWORD size, T_AlignType alignType);
T_VOID spinLockRaw(T_UWORD *lock);
T_VOID spinUnlockRaw(T_UWORD *lock);

extern int vprintk(const char *fmt, va_list ap);
extern int printk(const char *fmt, ...) __printf_like(1, 2);

#endif /* ASM_USE */

#ifdef __cplusplus
}
#endif /* COMMON_UTILS_H */

#endif
