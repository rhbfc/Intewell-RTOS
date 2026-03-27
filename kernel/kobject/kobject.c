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

#include <errno.h>
#include <kmalloc.h>
#include <kobject.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysfs.h>

#define KLOG_TAG "kobject"
#include <klog.h>

static void kobj_attach_to_kset(struct kobject *kobj);
static void kobj_detach_from_kset(struct kobject *kobj);
static void dynamic_kobj_release(struct kobject *kobj);

const struct sysfs_ops kobj_sysfs_ops;

static const struct kobj_type dynamic_kobj_type = {
    .release = dynamic_kobj_release,
    .sysfs_ops = &kobj_sysfs_ops,
};

static const struct kobj_type kset_kobj_type;

static void kobject_reset_state(struct kobject *kobj)
{
    INIT_LIST_HEAD(&kobj->entry);
    kref_init(&kobj->kref);
    kobj->sd = NULL;
    kobj->state_initialized = 1;
    kobj->state_in_sysfs = 0;
    kobj->state_add_uevent_sent = 0;
    kobj->state_remove_uevent_sent = 0;
    kobj->uevent_suppress = 0;
}

static struct kobject *kobject_parent_hold(struct kobject *kobj)
{
    struct kobject *parent = NULL;

    if (kobj->parent)
        parent = kobject_get(kobj->parent);

    if (!parent && kobj->kset)
        parent = kobject_get(&kobj->kset->kobj);

    return parent;
}

static int kobject_publish(struct kobject *kobj)
{
    struct kobject *parent;
    int ret;

    parent = kobject_parent_hold(kobj);
    kobj->parent = parent;

    if (kobj->kset)
        kobj_attach_to_kset(kobj);

    ret = sysfs_create_dir(kobj);
    if (ret)
    {
        if (kobj->kset)
            kobj_detach_from_kset(kobj);

        kobject_put(parent);
        kobj->parent = NULL;
        return ret;
    }

    kobj->state_in_sysfs = 1;
    return 0;
}

static int kobject_apply_name(struct kobject *kobj, const char *fmt, va_list vargs)
{
    char *name = NULL;

    if (!kobj || !fmt)
        return -EINVAL;

    vasprintf(&name, fmt, vargs);
    if (!name)
        return -ENOMEM;

    if (kobj->name)
        free((void *)kobj->name);

    kobj->name = name;
    return 0;
}

static int kobject_add_with_vargs(struct kobject *kobj, struct kobject *parent, const char *fmt,
                                  va_list vargs)
{
    int ret;

    ret = kobject_apply_name(kobj, fmt, vargs);
    if (ret)
        return ret;

    kobj->parent = parent;
    return kobject_publish(kobj);
}

static void dynamic_kobj_release(struct kobject *kobj)
{
    kfree(kobj);
}

static ssize_t kobj_attr_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
    struct kobj_attribute *kattr = container_of(attr, struct kobj_attribute, attr);

    if (!kattr->show)
        return -EIO;

    return kattr->show(kobj, kattr, buf);
}

static ssize_t kobj_attr_store(struct kobject *kobj, struct attribute *attr, const char *buf,
                               size_t count)
{
    struct kobj_attribute *kattr = container_of(attr, struct kobj_attribute, attr);

    if (!kattr->store)
        return -EIO;

    return kattr->store(kobj, kattr, buf, count);
}

const struct sysfs_ops kobj_sysfs_ops = {
    .show = kobj_attr_show,
    .store = kobj_attr_store,
};

void kobject_init(struct kobject *kobj, const struct kobj_type *ktype)
{
    if (!kobj || !ktype)
    {
        KLOG_E("kobject_init: invalid arguments");
        return;
    }

    kobject_reset_state(kobj);
    kobj->ktype = ktype;
}

int kobject_set_name_vargs(struct kobject *kobj, const char *fmt, va_list vargs)
{
    return kobject_apply_name(kobj, fmt, vargs);
}

int kobject_set_name(struct kobject *kobj, const char *fmt, ...)
{
    va_list vargs;
    int ret;

    va_start(vargs, fmt);
    ret = kobject_apply_name(kobj, fmt, vargs);
    va_end(vargs);

    return ret;
}

int kobject_add(struct kobject *kobj, struct kobject *parent, const char *fmt, ...)
{
    va_list vargs;
    int ret;

    if (!kobj || !fmt)
        return -EINVAL;

    if (!kobj->state_initialized || !kobj->ktype)
        return -EINVAL;

    va_start(vargs, fmt);
    ret = kobject_add_with_vargs(kobj, parent, fmt, vargs);
    va_end(vargs);

    return ret;
}

int kobject_init_and_add(struct kobject *kobj, const struct kobj_type *ktype,
                         struct kobject *parent, const char *fmt, ...)
{
    va_list vargs;
    int ret;

    if (!kobj || !ktype || !fmt)
        return -EINVAL;

    kobject_init(kobj, ktype);

    va_start(vargs, fmt);
    ret = kobject_add_with_vargs(kobj, parent, fmt, vargs);
    va_end(vargs);

    return ret;
}

static struct kobject *kobject_create(void)
{
    struct kobject *kobj = kzalloc(sizeof(*kobj), GFP_KERNEL);

    if (!kobj)
        return NULL;

    kobject_init(kobj, &dynamic_kobj_type);
    return kobj;
}

struct kobject *kobject_create_and_add(const char *name, struct kobject *parent)
{
    struct kobject *kobj;

    kobj = kobject_create();
    if (!kobj)
        return NULL;

    if (kobject_add(kobj, parent, "%s", name))
    {
        kobject_put(kobj);
        return NULL;
    }

    return kobj;
}

void kobject_del(struct kobject *kobj)
{
    struct kobject *parent;

    if (!kobj)
        return;

    parent = kobj->parent;

    if (kobj->state_in_sysfs)
    {
        sysfs_remove_dir(kobj);
        kobj->state_in_sysfs = 0;
    }

    if (kobj->kset)
        kobj_detach_from_kset(kobj);

    kobj->parent = NULL;
    kobject_put(parent);
}

struct kobject *kobject_get(struct kobject *kobj)
{
    if (kobj)
        kref_get(&kobj->kref);

    return kobj;
}

struct kobject *kobject_get_unless_zero(struct kobject *kobj)
{
    if (kobj && kref_get_unless_zero(&kobj->kref))
        return kobj;

    return NULL;
}

static void kobject_release_ref(struct kref *ref)
{
    struct kobject *kobj = container_of(ref, struct kobject, kref);

    if (kobj->ktype && kobj->ktype->release)
        kobj->ktype->release(kobj);
}

int kobject_put(struct kobject *kobj)
{
    if (!kobj)
        return 0;

    return kref_put(&kobj->kref, kobject_release_ref);
}

void kobject_get_ownership(const struct kobject *kobj, uid_t *uid, gid_t *gid)
{
    if (!uid || !gid)
        return;

    *uid = 0;
    *gid = 0;

    if (kobj && kobj->ktype && kobj->ktype->get_ownership)
        kobj->ktype->get_ownership(kobj, uid, gid);
}

void kset_init(struct kset *kset)
{
    if (!kset)
        return;

    kobject_reset_state(&kset->kobj);
    INIT_LIST_HEAD(&kset->list);
    spin_lock_init(&kset->list_lock);
}

int kset_register(struct kset *kset)
{
    if (!kset || !kset->kobj.ktype)
        return -EINVAL;

    kset_init(kset);
    return kobject_publish(&kset->kobj);
}

void kset_unregister(struct kset *kset)
{
    if (!kset)
        return;

    kobject_del(&kset->kobj);
    kobject_put(&kset->kobj);
}

static void kset_release(struct kobject *kobj)
{
    struct kset *kset = container_of(kobj, struct kset, kobj);

    if (kobj->name)
        free((void *)kobj->name);

    kfree(kset);
}

static void kset_inherit_ownership(const struct kobject *kobj, uid_t *uid, gid_t *gid)
{
    if (kobj && kobj->parent)
        kobject_get_ownership(kobj->parent, uid, gid);
}

static const struct kobj_type kset_kobj_type = {
    .release = kset_release,
    .sysfs_ops = &kobj_sysfs_ops,
    .get_ownership = kset_inherit_ownership,
};

static struct kset *kset_create(const char *name, const struct kset_uevent_ops *uevent_ops,
                                struct kobject *parent_kobj)
{
    struct kset *kset;

    kset = kzalloc(sizeof(*kset), GFP_KERNEL);
    if (!kset)
        return NULL;

    kset->uevent_ops = uevent_ops;
    kset->kobj.parent = parent_kobj;
    kset->kobj.kset = NULL;
    kset->kobj.ktype = &kset_kobj_type;

    if (kobject_set_name(&kset->kobj, "%s", name))
    {
        kfree(kset);
        return NULL;
    }

    return kset;
}

struct kset *kset_create_and_add(const char *name, const struct kset_uevent_ops *uevent_ops,
                                 struct kobject *parent_kobj)
{
    struct kset *kset = kset_create(name, uevent_ops, parent_kobj);

    if (!kset)
        return NULL;

    if (kset_register(kset))
    {
        if (kset->kobj.name)
            free((void *)kset->kobj.name);

        kfree(kset);
        return NULL;
    }

    return kset;
}

struct kobject *kset_find_obj(struct kset *kset, const char *name)
{
    struct kobject *kobj;

    if (!kset || !name)
        return NULL;

    spin_lock(&kset->list_lock);
    kset_for_each(kobj, kset)
    {
        const char *current_name = kobject_name(kobj);

        if (current_name && strcmp(current_name, name) == 0)
        {
            kobject_get(kobj);
            spin_unlock(&kset->list_lock);
            return kobj;
        }
    }
    spin_unlock(&kset->list_lock);

    return NULL;
}

static void kobj_attach_to_kset(struct kobject *kobj)
{
    struct kset *kset = kobj->kset;

    if (!kset)
        return;

    kset_get(kset);
    spin_lock(&kset->list_lock);
    list_add_tail(&kobj->entry, &kset->list);
    spin_unlock(&kset->list_lock);
}

static void kobj_detach_from_kset(struct kobject *kobj)
{
    struct kset *kset = kobj->kset;

    if (!kset)
        return;

    spin_lock(&kset->list_lock);
    if (kobj->entry.next && kobj->entry.prev)
        list_del(&kobj->entry);
    INIT_LIST_HEAD(&kobj->entry);
    spin_unlock(&kset->list_lock);

    kset_put(kset);
}
