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

#ifndef _UTIL_H_
#define _UTIL_H_

#include <system/macros.h>
#include <system/types.h>

#define DIV_ROUND_UP(n, d) (((n) + (d)-1) / (d))

#define __round_mask(x, y) ((__typeof__(x))((y)-1))
#define round_up(x, y) ((((x)-1) | __round_mask(x, y)) + 1)
#define round_down(x, y) ((x) & ~__round_mask(x, y))

#define __iomem volatile
#define READ_ONCE(var) (*((volatile typeof(var) *)(&(var))))
#define WRITE_ONCE(var, val) (READ_ONCE(var) = (val))
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

/* Ensure little-endian helpers available */
#ifndef cpu_to_le32
#define cpu_to_le32(x) ((u32)(x))
#endif
#ifndef le32_to_cpu
#define le32_to_cpu(x) ((u32)(x))
#endif
#ifndef cpu_to_le64
#define cpu_to_le64(x) ((u64)(x))
#endif
#ifndef le64_to_cpu
#define le64_to_cpu(x) ((u64)(x))
#endif
#ifndef cpu_to_le16
#define cpu_to_le16(x) ((u16)(x))
#endif
#ifndef le16_to_cpu
#define le16_to_cpu(x) ((u16)(x))
#endif

static inline __u32 __le32_to_cpup(const __le32 *p)
{
    return (__u32)*p;
}

static inline __u16 __le16_to_cpup(const __le16 *p)
{
    return (__u16)*p;
}
#define le32_to_cpup __le32_to_cpup
#define le16_to_cpup __le16_to_cpup

#ifndef min_t
#define min_t(type, a, b) ((type)(a) > (type)(b)) ? b : a
#endif

/**
 * clamp - return a value clamped to a given range with strict typechecking
 * @val: current value
 * @lo: lowest allowable value
 * @hi: highest allowable value
 *
 * This macro does strict typechecking of @lo/@hi to make sure they are of the
 * same type as @val.  See the unnecessary pointer comparisons.
 */
#define clamp(val, lo, hi) min((typeof(val))max(val, lo), hi)

#endif