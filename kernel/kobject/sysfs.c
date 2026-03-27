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
#include <kobject.h>
#include <string.h>
#include <sysfs.h>

#define KLOG_TAG "sysfs"
#include <klog.h>

static int sysfs_validate_object(const struct kobject *kobj)
{
    if (!kobj || !kobj->name)
        return -EINVAL;

    return 0;
}

int sysfs_create_dir(struct kobject *kobj)
{
    int ret;

    ret = sysfs_validate_object(kobj);
    if (ret < 0)
        return ret;

    kobj->state_in_sysfs = 1;
    KLOG_D("sysfs dir add: %s", kobj->name);

    return 0;
}

int sysfs_remove_dir(struct kobject *kobj)
{
    if (!kobj)
        return -EINVAL;

    if (!kobj->state_in_sysfs)
        return 0;

    kobj->state_in_sysfs = 0;
    KLOG_D("sysfs dir remove: %s", kobj->name ? kobj->name : "<unnamed>");

    return 0;
}

int sysfs_create_link(struct kobject *kobj, struct kobject *target, const char *name)
{
    if (!kobj || !target || !name)
        return -EINVAL;

    KLOG_D("sysfs link add: %s -> %s", name, target->name ? target->name : "<unnamed>");
    return 0;
}

int sysfs_remove_link(struct kobject *kobj, const char *name)
{
    if (!kobj || !name)
        return -EINVAL;

    KLOG_D("sysfs link remove: %s", name);
    return 0;
}

int sysfs_notify(struct kobject *kobj, const char *attr)
{
    if (!kobj)
        return -EINVAL;

    KLOG_D("sysfs notify: object=%s attr=%s", kobj->name ? kobj->name : "<unnamed>",
           attr ? attr : "<all>");
    return 0;
}

int sysfs_get_path(struct kobject *kobj, char *buf, size_t size)
{
    size_t used = 0;
    struct kobject *cursor;
    char *tail;

    if (!kobj || !buf || size < 2)
        return -EINVAL;

    tail = buf + size;
    *--tail = '\0';

    for (cursor = kobj; cursor; cursor = cursor->parent)
    {
        size_t name_len;

        if (!cursor->name)
            continue;

        name_len = strlen(cursor->name);
        if ((size_t)(tail - buf) <= name_len)
            return -ERANGE;

        tail -= name_len;
        memcpy(tail, cursor->name, name_len);
        used += name_len;

        if (cursor->parent)
        {
            if (tail == buf)
                return -ERANGE;

            *--tail = '/';
            used++;
        }
    }

    if ((size_t)(tail - buf) < 4)
        return -ERANGE;

    tail -= 4;
    memcpy(tail, "/sys", 4);
    used += 4;

    memmove(buf, tail, used + 1);
    return (int)used;
}

int sysfs_count_children(struct kobject *parent)
{
    int count = 0;
    struct kset *kset;
    struct kobject *kobj;

    if (!parent || !parent->kset)
        return 0;

    kset = parent->kset;
    kset_for_each(kobj, kset)
    {
        if (kobj->parent == parent)
            count++;
    }

    return count;
}
