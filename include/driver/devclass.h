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

#ifndef __DEVCLASS_H__
#define __DEVCLASS_H__
#include <limits.h>
#include <list.h>
#include <spinlock.h>
struct device;
struct devclass_type {
    struct list_head list;
    ttos_spinlock_t  lock;

    char           name[NAME_MAX];
    struct device *device;
};

#endif /* __DEVCLASS_H__ */
