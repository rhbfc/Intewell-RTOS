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
 * @file： atomic.h
 * @brief：
 *	    <li>提供原子操作相关接口定义。</li>
 */

#ifndef _ATOMIC_H
#define _ATOMIC_H

/************************头文件********************************/
#include <stdbool.h>
#include <stdint.h>
#include <system/compiler.h>
#include <system/types.h>
#include <util.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/************************类型定义******************************/

/* atomic32_t类型定义 */
typedef struct
{
    volatile s32 counter;
} atomic32_t;

typedef struct
{
    volatile s64 counter;
} atomic64_t;

#if UINTPTR_MAX == 0xffffffffUL
typedef atomic32_t atomic_t;
#define atomic_cas atomic32_cas
#define atomic_add atomic32_add
#define atomic_add_return atomic32_add_return
#define atomic_fetch_add atomic32_fetch_add
#define atomic_sub atomic32_sub
#define atomic_sub_return atomic32_sub_return
#define atomic_inc atomic32_inc
#define atomic_inc_return atomic32_inc_return
#define atomic_dec atomic32_dec
#define atomic_dec_return atomic32_dec_return
#define atomic_set atomic32_set
#else
typedef atomic64_t atomic_t;
#define atomic_cas atomic64_cas
#define atomic_add atomic64_add
#define atomic_add_return atomic64_add_return
#define atomic_fetch_add atomic64_fetch_add
#define atomic_sub atomic64_sub
#define atomic_sub_return atomic64_sub_return
#define atomic_inc atomic64_inc
#define atomic_inc_return atomic64_inc_return
#define atomic_dec atomic64_dec
#define atomic_dec_return atomic64_dec_return
#define atomic_set atomic64_set
#endif

/************************接口声明******************************/
bool atomic32_cas(atomic32_t *target, s32 oldValue, s32 newValue);
s32 atomic32_add(atomic32_t *target, s32 value);
s32 atomic32_add_return(atomic32_t *target, s32 value);
s32 atomic32_fetch_add(atomic32_t *target, s32 value);
s32 atomic32_sub(atomic32_t *target, s32 value);
s32 atomic32_sub_return(atomic32_t *target, s32 value);
s32 atomic32_inc(atomic32_t *target);
s32 atomic32_inc_return(atomic32_t *target);
s32 atomic32_dec(atomic32_t *target);
s32 atomic32_dec_return(atomic32_t *target);
s32 atomic32_set(atomic32_t *target, s32 value);

bool atomic64_cas(atomic64_t *target, s64 oldValue, s64 newValue);
s64 atomic64_add(atomic64_t *target, s64 value);
s64 atomic64_add_return(atomic64_t *target, s64 value);
s64 atomic64_fetch_add(atomic64_t *target, s64 value);
s64 atomic64_sub(atomic64_t *target, s64 value);
s64 atomic64_sub_return(atomic64_t *target, s64 value);
s64 atomic64_inc(atomic64_t *target);
s64 atomic64_inc_return(atomic64_t *target);
s64 atomic64_dec(atomic64_t *target);
s64 atomic64_dec_return(atomic64_t *target);
s64 atomic64_set(atomic64_t *target, s64 value);

/************************接口定义******************************/

#define ATOMIC_INIT(v, val) (v)->counter = (val)

#define ATOMIC_INITIALIZER(val)                                                                    \
    {                                                                                              \
        .counter = (val),                                                                          \
    }

/**
 * @brief
 *    原子读操作
 * @param[in] v 需要读取值的地址
 * @retval 无
 */
#define atomic_read(v) READ_ONCE((v)->counter)

/**
 * @brief
 *    原子写操作
 * @param[in] v 需要设置值的地址
 * @param[in] value 需要设置的值
 * @retval 无
 */
#define atomic_write(v, value) WRITE_ONCE((v)->counter, value)

#define atomic32_write atomic_write
#define atomic64_write atomic_write

#define atomic32_read atomic_read
#define atomic64_read atomic_read

#ifndef atomic_dec_if_positive
static inline long atomic_dec_if_positive(atomic_t *v)
{
    long dec, c;
    do
    {
        c = atomic_read(v);
        dec = c - 1;
        if (unlikely(dec < 0))
            break;
    } while (!atomic_cas(v, c, dec));

    return dec;
}
#define atomic_dec_if_positive atomic_dec_if_positive
#endif

#ifndef atomic_fetch_add_unless
static inline long atomic_fetch_add_unless(atomic_t *v, long a, long u)
{
    long c;
    do
    {
        c = atomic_read(v);
        if (unlikely(c == u))
            break;
    } while (!atomic_cas(v, c, c + a));

    return c;
}
#define atomic_fetch_add_unless atomic_fetch_add_unless
#endif

#ifndef atomic_add_unless
static inline long atomic_add_unless(atomic_t *v, long a, long u)
{
    return atomic_fetch_add_unless(v, a, u) != u;
}
#define atomic_add_unless atomic_add_unless
#endif

#ifndef atomic_inc_not_zero
static inline long atomic_inc_not_zero(atomic_t *v)
{
    return atomic_add_unless(v, 1, 0);
}
#define atomic_inc_not_zero atomic_inc_not_zero
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _ATOMIC_H */
