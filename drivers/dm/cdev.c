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

/**
 * @file char_dev.c
 * @brief 字符设备框架实现
 */

#include <driver/cdev.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <spinlock.h>
#include <list.h>
#include <fs/fs.h>
#include <ttos_init.h>
#include <sys/stat.h>

/**
 * @brief 字符设备管理结构
 */
static struct
{
    struct list_head device_list; /* 设备链表 */
    ttos_spinlock_t lock;         /* 设备锁 */
} char_device_map;

/**
 * @brief 锁定字符设备管理系统
 */
static void lock_char_dev(irq_flags_t *flags)
{
    spin_lock_irqsave(&char_device_map.lock, *flags);
}

/**
 * @brief 解锁字符设备管理系统
 */
static void unlock_char_dev(irq_flags_t *flags)
{
    spin_unlock_irqrestore(&char_device_map.lock, *flags);
}

/**
 * @brief 查找字符设备
 *
 * @param dev 设备号
 * @return 成功返回设备指针，失败返回NULL
 */
static struct char_device *find_char_device(dev_t dev)
{
    struct char_device *cdev;

    list_for_each_entry(cdev, &char_device_map.device_list, list)
    {
        if (MAJOR(cdev->dev) == MAJOR(dev) && MINOR(dev) >= MINOR(cdev->dev) &&
            MINOR(dev) < MINOR(cdev->dev) + cdev->count)
        {
            return cdev;
        }
    }

    return NULL;
}

int cdev_init(struct char_device *cdev, const struct file_operations *fops)
{
    if (!cdev || !fops)
    {
        return -1;
    }

    memset(cdev, 0, sizeof(*cdev));
    cdev->ops = fops;
    INIT_LIST_HEAD(&cdev->list);

    return 0;
}

int cdev_add(struct char_device *cdev, dev_t dev, unsigned int count)
{
    irq_flags_t flags;
    if (!cdev || count == 0 || count > MAX_MINOR + 1)
    {
        return -1;
    }

    cdev->dev = dev;
    cdev->count = count;

    lock_char_dev(&flags);

    /* 检查设备号是否已被使用 */
    if (find_char_device(dev))
    {
        unlock_char_dev(&flags);
        return -1;
    }

    /* 添加到设备链表 */
    list_add(&cdev->list, &char_device_map.device_list);

    unlock_char_dev(&flags);
    return 0;
}

void cdev_del(struct char_device *cdev)
{
    irq_flags_t flags;
    if (!cdev)
    {
        return;
    }

    lock_char_dev(&flags);

    /* 从设备链表中删除 */
    list_del(&cdev->list);

    unlock_char_dev(&flags);
}

/**
 * @brief 默认字符设备结构
 */
struct default_char_device
{
    struct char_device cdev;
    const struct file_operations *fops;
};

int register_chrdev(unsigned int major, const char *name, const struct file_operations *fops)
{
    dev_t dev;
    struct default_char_device *def_cdev;
    int ret;
    char path[64] = "/dev/";
    unsigned int minor;

    if (!name || !fops)
    {
        return -1;
    }

    /* 分配设备号 */
    if (major)
    {
        /* 分配次设备号 */
        for(minor = 0; minor <= MAX_MINOR; minor++)
        {
            dev = MKDEV(major, minor);
            if (!find_char_device(dev))
            {
                break;
            }
        }
        if (minor > MAX_MINOR)
        {
            return -1;
        }
        ret = register_chrdev_region(MKDEV(major, minor), 1, name);
        if (ret < 0)
        {
            return ret;
        }
        dev = MKDEV(major, minor);
    }
    else
    {
        ret = alloc_chrdev_region(&dev, 0, 1, name);
        if (ret < 0)
        {
            return ret;
        }
    }

    /* 分配默认字符设备结构 */
    def_cdev = malloc(sizeof(*def_cdev));
    if (!def_cdev)
    {
        unregister_chrdev_region(dev, 1);
        return -1;
    }

    /* 初始化字符设备 */
    cdev_init(&def_cdev->cdev, fops);
    def_cdev->cdev.dev = dev;
    def_cdev->cdev.count = 1;
    strncpy(def_cdev->cdev.name, name, sizeof(def_cdev->cdev.name) - 1);
    def_cdev->cdev.name[sizeof(def_cdev->cdev.name) - 1] = '\0';
    def_cdev->fops = fops;

    /* 添加字符设备 */
    ret = cdev_add(&def_cdev->cdev, dev, 1);
    if (ret < 0)
    {
        free(def_cdev);
        unregister_chrdev_region(dev, 1);
        return ret;
    }
    snprintf(path, sizeof(path), "/dev/%s%d", name, MINOR(dev));

    vfs_bind_path(path, fops, dev, 0666 | S_IFCHR, NULL);

    return MAJOR(dev);
}

void unregister_chrdev(unsigned int major, const char *name)
{
    struct char_device *cdev, *tmp, *n;
    irq_flags_t flags;
    dev_t dev = MKDEV(major, 0);

    lock_char_dev(&flags);

    /* 查找并删除设备 */
    list_for_each_entry_safe(cdev, n, &char_device_map.device_list, list)
    {
        if (MAJOR(cdev->dev) == major && (name == NULL || strcmp(cdev->name, name) == 0))
        {
            list_del(&cdev->list);
            tmp = cdev;
            if (container_of(tmp, struct default_char_device, cdev))
            {
                free(container_of(tmp, struct default_char_device, cdev));
            }
        }
    }

    unlock_char_dev(&flags);

    /* 释放设备号 */
    unregister_chrdev_region(dev, 1);
}

static int char_dev_init(void)
{
    /* 初始化字符设备管理结构 */
    INIT_LIST_HEAD(&char_device_map.device_list);

    /* 初始化锁 */
    spin_lock_init(&char_device_map.lock);

    return 0;
}
INIT_EXPORT_PRE_DRIVER(char_dev_init, "char dev init");
