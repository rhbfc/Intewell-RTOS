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
 * @file： barrier.h
 * @brief：
 *	    <li>栅栏相关操作宏定义。</li>
 */
#ifndef _BARRIER_H
#define _BARRIER_H

#include <arch_helpers.h>

/************************头文件********************************/

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/************************宏定义********************************/
#ifndef ASM_USE

#define mb()  dmbsy ()
#define rmb() dmbsy ()
#define wmb() dmbsy ()

#define smp_mb()  dmbish()
#define smp_rmb() smp_mb ()
#define smp_wmb() dmbishst()

/* AArch64：用 ldar，根据读取宽度选择指令 */
#define smp_load_acquire(p)                                                 \
    ({                                                                      \
        typeof(*(p)) __val;                                                 \
        if (__builtin_types_compatible_p(typeof(__val), uint8_t) ||         \
            __builtin_types_compatible_p(typeof(__val), int8_t)) {          \
            unsigned int __tmp;                                             \
            __asm__ __volatile__("ldarb %w0, %1"                            \
                                 : "=r"(__tmp)                              \
                                 : "Q"(*(p))                                \
                                 : "memory");                               \
            __val = (typeof(__val))__tmp;                                   \
        } else if (__builtin_types_compatible_p(typeof(__val), uint16_t) || \
                   __builtin_types_compatible_p(typeof(__val), int16_t)) {  \
            unsigned int __tmp;                                             \
            __asm__ __volatile__("ldarh %w0, %1"                            \
                                 : "=r"(__tmp)                              \
                                 : "Q"(*(p))                                \
                                 : "memory");                               \
            __val = (typeof(__val))__tmp;                                   \
        } else if (sizeof(__val) == 4) {                                     \
            __asm__ __volatile__("ldar %w0, %1"                             \
                                 : "=r"(__val)                              \
                                 : "Q"(*(p))                                \
                                 : "memory");                               \
        } else if (sizeof(__val) == 8) {                                     \
            __asm__ __volatile__("ldar %0, %1"                              \
                                 : "=r"(__val)                              \
                                 : "Q"(*(p))                                \
                                 : "memory");                               \
        } else {                                                             \
            /* 其它大小不支持：你可以改成 memcpy + dmb ish */               \
            __val = *(p);                                                    \
            smp_mb();                                                        \
        }                                                                    \
        __val;                                                               \
    })

/* AArch64：用 stlr，根据写入宽度选择指令 */
#define smp_store_release(p, v)                                              \
    do {                                                                      \
        typeof(*(p)) __val = (v);                                             \
        /* stlrb/stlrh/stlr 只接受寄存器操作数 */                             \
        if (__builtin_types_compatible_p(typeof(__val), uint8_t) ||           \
            __builtin_types_compatible_p(typeof(__val), int8_t)) {            \
            __asm__ __volatile__("stlrb %w1, %0"                              \
                                 : "=Q"(*(p))                                 \
                                 : "r"(__val)                                 \
                                 : "memory");                                 \
        } else if (__builtin_types_compatible_p(typeof(__val), uint16_t) ||   \
                   __builtin_types_compatible_p(typeof(__val), int16_t)) {    \
            __asm__ __volatile__("stlrh %w1, %0"                              \
                                 : "=Q"(*(p))                                 \
                                 : "r"(__val)                                 \
                                 : "memory");                                 \
        } else if (sizeof(__val) == 4) {                                      \
            __asm__ __volatile__("stlr %w1, %0"                               \
                                 : "=Q"(*(p))                                 \
                                 : "r"(__val)                                 \
                                 : "memory");                                 \
        } else if (sizeof(__val) == 8) {                                      \
            __asm__ __volatile__("stlr %1, %0"                                \
                                 : "=Q"(*(p))                                 \
                                 : "r"(__val)                                 \
                                 : "memory");                                 \
        } else {                                                               \
            /* 其它大小不支持：你可以改成 memcpy + dmb ish */                 \
            smp_mb();                                                         \
            *(p) = __val;                                                     \
        }                                                                     \
    } while (0)
    

#endif
/************************类型定义******************************/

/************************接口声明******************************/

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* _BARRIER_H */
