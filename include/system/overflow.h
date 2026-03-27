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

#ifndef __OVERFLOW_H
#define __OVERFLOW_H

#include <system/compiler.h>

/*
 * Allows for effectively applying __must_check to a macro so we can have
 * both the type-agnostic benefits of the macros while also being able to
 * enforce that the return value is, in fact, checked.
 */
static inline bool __must_check __must_check_overflow(bool overflow)
{
	return unlikely(overflow);
}

/**
 * check_add_overflow() - Calculate addition with overflow checking
 * @a: first addend
 * @b: second addend
 * @d: pointer to store sum
 *
 * Returns true on wrap-around, false otherwise.
 *
 * *@d holds the results of the attempted addition, regardless of whether
 * wrap-around occurred.
 */
#define check_add_overflow(a, b, d)	\
	__must_check_overflow(__builtin_add_overflow(a, b, d))


#endif /* __LINUX_OVERFLOW_H */
