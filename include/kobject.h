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

#ifndef _KOBJECT_H
#define _KOBJECT_H

#include <kref.h>
#include <list.h>
#include <spinlock.h>
#include <stdarg.h>
#include <stdio.h>
#include <sysfs.h>
#include <system/compiler.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UEVENT_HELPER_PATH_LEN 256
#define UEVENT_NUM_ENVP 64
#define UEVENT_BUFFER_SIZE 2048

struct kobject;
struct kset;
struct kobj_type;
struct kset_uevent_ops;

struct kobj_attribute
{
    struct attribute attr;
    ssize_t (*show)(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
    ssize_t (*store)(struct kobject *kobj, struct kobj_attribute *attr, const char *buf,
                     size_t count);
};

struct kobj_type
{
    void (*release)(struct kobject *kobj);
    const struct sysfs_ops *sysfs_ops;
    const struct attribute_group **default_groups;
    void (*get_ownership)(const struct kobject *kobj, uid_t *uid, gid_t *gid);
};

struct kobject
{
    const char *name;
    struct list_head entry;
    struct kobject *parent;
    struct kset *kset;
    const struct kobj_type *ktype;
    struct kref kref;
    void *sd;

    unsigned int state_initialized : 1;
    unsigned int state_in_sysfs : 1;
    unsigned int state_add_uevent_sent : 1;
    unsigned int state_remove_uevent_sent : 1;
    unsigned int uevent_suppress : 1;
};

struct kset
{
    struct list_head list;
    ttos_spinlock_t list_lock;
    struct kobject kobj;
    const struct kset_uevent_ops *uevent_ops;
};

struct kobj_uevent_env
{
    char *argv[3];
    char *envp[UEVENT_NUM_ENVP];
    int envp_idx;
    char buf[UEVENT_BUFFER_SIZE];
    int buflen;
};

struct kset_uevent_ops
{
    int (*const filter)(const struct kobject *kobj);
    const char *(*const name)(const struct kobject *kobj);
    int (*const uevent)(const struct kobject *kobj, struct kobj_uevent_env *env);
};

#define KOBJECT_INIT_NAME(name)                                                                    \
    {                                                                                              \
        .name = (name), .entry = LIST_HEAD_INIT(entry), .parent = NULL, .kset = NULL,              \
        .ktype = NULL, .kref = KREF_INIT(1), .sd = NULL, .state_initialized = 0,                   \
        .state_in_sysfs = 0, .state_add_uevent_sent = 0, .state_remove_uevent_sent = 0,            \
        .uevent_suppress = 0,                                                                      \
    }

__printf_like(2, 3) int kobject_set_name(struct kobject *kobj, const char *fmt, ...);
__printf_like(2, 0) int kobject_set_name_vargs(struct kobject *kobj, const char *fmt,
                                               va_list vargs);

void kobject_init(struct kobject *kobj, const struct kobj_type *ktype);

__printf_like(3, 4) __must_check
    int kobject_add(struct kobject *kobj, struct kobject *parent, const char *fmt, ...);

__printf_like(4, 5) __must_check
    int kobject_init_and_add(struct kobject *kobj, const struct kobj_type *ktype,
                             struct kobject *parent, const char *fmt, ...);

__must_check struct kobject *kobject_create_and_add(const char *name, struct kobject *parent);

void kobject_del(struct kobject *kobj);
struct kobject *kobject_get(struct kobject *kobj);
struct kobject *kobject_get_unless_zero(struct kobject *kobj);
int kobject_put(struct kobject *kobj);
void kobject_get_ownership(const struct kobject *kobj, uid_t *uid, gid_t *gid);

static inline struct kset *to_kset(struct kobject *kobj)
{
    return kobj ? container_of(kobj, struct kset, kobj) : NULL;
}

static inline struct kset *kset_get(struct kset *k)
{
    return k ? to_kset(kobject_get(&k->kobj)) : NULL;
}

static inline void kset_put(struct kset *k)
{
    if (k)
        kobject_put(&k->kobj);
}

static inline const struct kobj_type *get_ktype(const struct kobject *kobj)
{
    return kobj ? kobj->ktype : NULL;
}

struct kset *kset_create_and_add(const char *name, const struct kset_uevent_ops *uevent_ops,
                                 struct kobject *parent_kobj);
int kset_register(struct kset *kset);
void kset_unregister(struct kset *kset);
struct kobject *kset_find_obj(struct kset *kset, const char *name);

static inline const char *kobject_name(const struct kobject *kobj)
{
    return kobj ? kobj->name : NULL;
}

#define container_of_kref(kref, type, member) container_of((kref), type, member.kref)

#define kset_for_each(kobj, kset) list_for_each_entry(kobj, &(kset)->list, entry)
#define kset_for_each_safe(kobj, n, kset) list_for_each_entry_safe(kobj, n, &(kset)->list, entry)

#ifdef __cplusplus
}
#endif

#endif /* _KOBJECT_H */
