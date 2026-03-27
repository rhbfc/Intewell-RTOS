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

#ifndef __NON_ATOMIC_H__
#define __NON_ATOMIC_H__

#include <stdbool.h>
#include <system/bitops.h>

static __always_inline unsigned long *non_atomic_bit_word(volatile unsigned long *addr,
                                                          unsigned long nr)
{
    return ((unsigned long *)addr) + BIT_WORD(nr);
}

static __always_inline const unsigned long *
non_atomic_const_bit_word(const volatile unsigned long *addr, unsigned long nr)
{
    return ((const unsigned long *)addr) + BIT_WORD(nr);
}

static __always_inline unsigned long non_atomic_bit_mask(unsigned long nr)
{
    return BIT_MASK(nr);
}

static __always_inline void non_atomic_set_bit_impl(unsigned long nr, volatile unsigned long *addr)
{
    unsigned long *word = non_atomic_bit_word(addr, nr);

    *word |= non_atomic_bit_mask(nr);
}

static __always_inline void non_atomic_clear_bit_impl(unsigned long nr,
                                                      volatile unsigned long *addr)
{
    unsigned long *word = non_atomic_bit_word(addr, nr);

    *word &= ~non_atomic_bit_mask(nr);
}

static __always_inline void non_atomic_change_bit_impl(unsigned long nr,
                                                       volatile unsigned long *addr)
{
    unsigned long *word = non_atomic_bit_word(addr, nr);

    *word ^= non_atomic_bit_mask(nr);
}

static __always_inline bool non_atomic_test_and_set_bit_impl(unsigned long nr,
                                                             volatile unsigned long *addr)
{
    unsigned long *word = non_atomic_bit_word(addr, nr);
    unsigned long mask = non_atomic_bit_mask(nr);
    unsigned long old = *word;

    *word = old | mask;
    return (old & mask) != 0;
}

static __always_inline bool non_atomic_test_and_clear_bit_impl(unsigned long nr,
                                                               volatile unsigned long *addr)
{
    unsigned long *word = non_atomic_bit_word(addr, nr);
    unsigned long mask = non_atomic_bit_mask(nr);
    unsigned long old = *word;

    *word = old & ~mask;
    return (old & mask) != 0;
}

static __always_inline bool non_atomic_test_and_change_bit_impl(unsigned long nr,
                                                                volatile unsigned long *addr)
{
    unsigned long *word = non_atomic_bit_word(addr, nr);
    unsigned long mask = non_atomic_bit_mask(nr);
    unsigned long old = *word;

    *word = old ^ mask;
    return (old & mask) != 0;
}

static __always_inline bool non_atomic_test_bit_impl(unsigned long nr,
                                                     const volatile unsigned long *addr)
{
    const unsigned long *word = non_atomic_const_bit_word(addr, nr);

    return ((*word >> BIT_WORD_OFFSET(nr)) & 1UL) != 0;
}

#ifndef CONFIG_SMP
#define NON_ATOMIC_BITOP(name, nr, addr)                                                           \
    (__builtin_constant_p(nr) ? ____atomic_##name((nr), (addr)) : _##name((nr), (addr)))
#else
#define NON_ATOMIC_BITOP(name, nr, addr) __##name((nr), (addr))
#endif

#define set_bit(nr, addr) NON_ATOMIC_BITOP(set_bit, nr, addr)
#define clear_bit(nr, addr) NON_ATOMIC_BITOP(clear_bit, nr, addr)
#define change_bit(nr, addr) NON_ATOMIC_BITOP(change_bit, nr, addr)
#define test_and_set_bit(nr, addr) NON_ATOMIC_BITOP(test_and_set_bit, nr, addr)
#define test_and_clear_bit(nr, addr) NON_ATOMIC_BITOP(test_and_clear_bit, nr, addr)
#define test_and_change_bit(nr, addr) NON_ATOMIC_BITOP(test_and_change_bit, nr, addr)

#define __set_bit(nr, addr) non_atomic_set_bit_impl((nr), (addr))
#define __clear_bit(nr, addr) non_atomic_clear_bit_impl((nr), (addr))
#define __change_bit(nr, addr) non_atomic_change_bit_impl((nr), (addr))
#define __test_and_set_bit(nr, addr) non_atomic_test_and_set_bit_impl((nr), (addr))
#define __test_and_clear_bit(nr, addr) non_atomic_test_and_clear_bit_impl((nr), (addr))
#define __test_and_change_bit(nr, addr) non_atomic_test_and_change_bit_impl((nr), (addr))
#define test_bit(nr, addr) non_atomic_test_bit_impl((nr), (addr))

#endif /* __NON_ATOMIC_H__ */
