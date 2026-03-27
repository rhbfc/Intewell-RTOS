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

#ifndef __KREF_H__
#define __KREF_H__

#include <atomic.h>

#ifdef __cplusplus
extern "C" {
#endif

struct kref
{
    atomic_t refcount;
};

#define KREF_INIT(val)                                                                             \
    {                                                                                              \
        .refcount = ATOMIC_INITIALIZER(val)                                                        \
    }

static inline void kref_init(struct kref *kref)
{
    atomic_set(&kref->refcount, 1);
}

static inline int kref_read(const struct kref *kref)
{
    return atomic_read((atomic_t *)&kref->refcount);
}

static inline void kref_get(struct kref *kref)
{
    atomic_inc(&kref->refcount);
}

static inline int kref_put(struct kref *kref, void (*release)(struct kref *kref))
{
    if (atomic_sub_return(&kref->refcount, 1) == 0)
    {
        release(kref);
        return 1;
    }

    return 0;
}

static inline int kref_get_unless_zero(struct kref *kref)
{
    return atomic_inc_not_zero(&kref->refcount) ? 1 : 0;
}

#ifdef __cplusplus
}
#endif

#endif /* __KREF_H__ */
