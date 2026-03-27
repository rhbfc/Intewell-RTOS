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
 * @file： int.h
 * @brief：
 *	    <li>cpu中断操作相关接口。</li>
 */
#ifndef _INT_H
#define _INT_H

/************************头文件********************************/
#include <system/types.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/************************宏定义********************************/

/* 使能CPU中断 */
#define arch_cpu_int_enable()                                                  \
    do                                                                         \
    {                                                                          \
        asm volatile("msr daifclr, #3" ::: "memory");                                 \
    } while (0)

/* 禁止CPU中断 */
#define arch_cpu_int_disable()                                                 \
    do                                                                         \
    {                                                                          \
        asm volatile("msr daifset, #3" ::: "memory");                                 \
    } while (0)

/* 禁止CPU中断 */
#define arch_cpu_int_save(msr)                                                 \
    do                                                                         \
    {                                                                          \
        asm volatile("mrs %0, daif\n\t"                                        \
                     "msr daifset, #3\n\t"                                            \
                     : "=r"(msr)                                               \
                     :                                                         \
                     : "memory", "cc");                                        \
    } while (0)

/* 恢复CPU中断 */
#define arch_cpu_int_restore(msr)                                              \
    do                                                                         \
    {                                                                          \
        asm volatile("msr daif, %0\n\t" ::"r"(msr) : "memory", "cc");        \
    } while (0)

/************************类型定义******************************/

/************************接口声明******************************/

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _INT_H */
