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

#ifndef _SYSFS_H
#define _SYSFS_H

#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/stat.h>
#include <system/compiler.h>

#define VERIFY_OCTAL_PERMISSIONS(mode) (mode)

#ifdef __cplusplus
extern "C" {
#endif

struct kobject;
struct kset;
struct file;
struct dirent;
struct inode;
struct bin_attribute;

struct attribute
{
    const char *name;
    mode_t mode;
};

struct sysfs_ops
{
    ssize_t (*show)(struct kobject *, struct attribute *, char *);
    ssize_t (*store)(struct kobject *, struct attribute *, const char *, size_t);
};

struct attribute_group
{
    const char *name;
    mode_t (*is_visible)(struct kobject *, struct attribute *, int);
    mode_t (*is_bin_visible)(struct kobject *, const struct bin_attribute *, int);
    size_t (*bin_size)(struct kobject *, const struct bin_attribute *, int);
    struct attribute **attrs;
    union {
        struct bin_attribute **bin_attrs;
        const struct bin_attribute *const *bin_attrs_new;
    };
};

struct bin_attribute
{
    struct attribute attr;
    size_t size;
    void *private;
    ssize_t (*read)(struct file *, struct kobject *, struct bin_attribute *, char *, off_t, size_t);
    ssize_t (*read_new)(struct file *, struct kobject *, const struct bin_attribute *, char *,
                        off_t, size_t);
    ssize_t (*write)(struct file *, struct kobject *, struct bin_attribute *, char *, off_t,
                     size_t);
    ssize_t (*write_new)(struct file *, struct kobject *, const struct bin_attribute *, char *,
                         off_t, size_t);
    off_t (*llseek)(struct file *, struct kobject *, const struct bin_attribute *, off_t, int);
};

#define __ATTR(_name, _mode, _show, _store)                                                        \
    {                                                                                              \
        .attr = {.name = __stringify(_name), .mode = VERIFY_OCTAL_PERMISSIONS(_mode)},             \
        .show = _show, .store = _store,                                                            \
    }

#define __ATTR_PREALLOC(_name, _mode, _show, _store)                                               \
    {                                                                                              \
        .attr = {.name = __stringify(_name),                                                       \
                 .mode = SYSFS_PREALLOC | VERIFY_OCTAL_PERMISSIONS(_mode)},                        \
        .show = _show, .store = _store,                                                            \
    }

#define __ATTR_RO(_name)                                                                           \
    {                                                                                              \
        .attr = {.name = __stringify(_name), .mode = 0444}, .show = _name##_show,                  \
    }

#define __ATTR_RO_MODE(_name, _mode)                                                               \
    {                                                                                              \
        .attr = {.name = __stringify(_name), .mode = VERIFY_OCTAL_PERMISSIONS(_mode)},             \
        .show = _name##_show,                                                                      \
    }

#define __ATTR_RW_MODE(_name, _mode)                                                               \
    {                                                                                              \
        .attr = {.name = __stringify(_name), .mode = VERIFY_OCTAL_PERMISSIONS(_mode)},             \
        .show = _name##_show, .store = _name##_store,                                              \
    }

#define __ATTR_WO(_name)                                                                           \
    {                                                                                              \
        .attr = {.name = __stringify(_name), .mode = 0200}, .store = _name##_store,                \
    }

#define __ATTR_RW(_name) __ATTR(_name, 0644, _name##_show, _name##_store)

#define __ATTR_NULL                                                                                \
    {                                                                                              \
        .attr = {.name = NULL }                                                                    \
    }

#ifdef CONFIG_DEBUG_LOCK_ALLOC
#define __ATTR_IGNORE_LOCKDEP(_name, _mode, _show, _store)                                         \
    {                                                                                              \
        .attr = {.name = __stringify(_name), .mode = _mode, .ignore_lockdep = true},               \
        .show = _show, .store = _store,                                                            \
    }
#else
#define __ATTR_IGNORE_LOCKDEP __ATTR
#endif

#define __ATTRIBUTE_GROUPS(_name)                                                                  \
    static const struct attribute_group *_name##_groups[] = {                                      \
        &_name##_group,                                                                            \
        NULL,                                                                                      \
    }

#define ATTRIBUTE_GROUPS(_name)                                                                    \
    static const struct attribute_group _name##_group = {                                          \
        .attrs = _name##_attrs,                                                                    \
    };                                                                                             \
    __ATTRIBUTE_GROUPS(_name)

#define BIN_ATTRIBUTE_GROUPS(_name)                                                                \
    static const struct attribute_group _name##_group = {                                          \
        .bin_attrs_new = _name##_attrs,                                                            \
    };                                                                                             \
    __ATTRIBUTE_GROUPS(_name)

int sysfs_create_dir(struct kobject *kobj);
int sysfs_remove_dir(struct kobject *kobj);

int sysfs_create_link(struct kobject *kobj, struct kobject *target, const char *name);
int sysfs_remove_link(struct kobject *kobj, const char *name);

int sysfs_notify(struct kobject *kobj, const char *attr);
int sysfs_get_path(struct kobject *kobj, char *buf, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* _SYSFS_H */
