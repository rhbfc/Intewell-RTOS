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

#ifndef __DRIVER_H__
#define __DRIVER_H__

#include <limits.h>
#include <list.h>
#include <stdint.h>
#include <sysfs.h>

struct bus_type;
struct of_device_id;
struct acpi_device_id;
struct device;
struct driver_private;

struct driver
{
    /* Private fields (for device driver framework) */
    struct list_head head;
    /* Public fields */
    char name[NAME_MAX];
    struct bus_type *bus;
    const struct of_device_id *of_match_table;
    const struct acpi_device_id *acpi_match_table;
    int (*probe)(struct device *);
    int (*bind)(struct device *dev);
    int (*suspend)(struct device *, uint32_t);
    int (*resume)(struct device *);
    int (*remove)(struct device *);
    int (*ofdata_to_platdata)(struct device *dev);

    void *ops;

    struct driver_private *p;

    size_t priv_auto_alloc_size;
    size_t platdata_auto_alloc_size;
};

struct driver_attribute
{
    struct attribute attr;
    ssize_t (*show)(struct driver *driver, char *buf);
    ssize_t (*store)(struct driver *driver, const char *buf, size_t count);
};

#define DRIVER_ATTR_RW(_name) struct driver_attribute driver_attr_##_name = __ATTR_RW(_name)
#define DRIVER_ATTR_RO(_name) struct driver_attribute driver_attr_##_name = __ATTR_RO(_name)
#define DRIVER_ATTR_WO(_name) struct driver_attribute driver_attr_##_name = __ATTR_WO(_name)

#endif /* __DRIVER_H__ */
