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

#ifndef __UNALIGNED_H__
#define __UNALIGNED_H__

#include <stdint.h>
#include <string.h>

#define __get_unaligned_t(type, ptr)                                                               \
    ({                                                                                             \
        type __value;                                                                              \
        memcpy(&__value, (ptr), sizeof(__value));                                                  \
        __value;                                                                                   \
    })

#define __put_unaligned_t(type, val, ptr)                                                          \
    do                                                                                             \
    {                                                                                              \
        type __value = (type)(val);                                                                \
        memcpy((ptr), &__value, sizeof(__value));                                                  \
    } while (0)

#define get_unaligned(ptr) __get_unaligned_t(typeof(*(ptr)), (ptr))
#define put_unaligned(val, ptr) __put_unaligned_t(typeof(*(ptr)), (val), (ptr))

static inline uint16_t get_unaligned_16(const void *p)
{
    return __get_unaligned_t(uint16_t, p);
}

static inline uint32_t get_unaligned_32(const void *p)
{
    return __get_unaligned_t(uint32_t, p);
}

static inline uint64_t get_unaligned_64(const void *p)
{
    return __get_unaligned_t(uint64_t, p);
}

#endif /* __UNALIGNED_H__ */
