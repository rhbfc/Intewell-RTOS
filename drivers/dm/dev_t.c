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
 * @file dev_t.c
 * @brief 设备号管理系统实现
 */

#include <driver/dev_t.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <spinlock.h>
#include <ttos_init.h>

#include <commonUtils.h>

/**
 * @brief 设备号位图结构
 */
typedef struct
{
    unsigned int major;    /* 主设备号 */
    unsigned long *bitmap; /* 次设备号位图 */
    char name[32];         /* 设备名称 */
} major_bitmap_t;

/**
 * @brief 主设备号管理结构
 */
struct major_map
{
    major_bitmap_t *major_bitmaps; /* 主设备号位图数组 */
    unsigned int count;            /* 已分配的主设备号数量 */
    unsigned int capacity;         /* 位图数组容量 */
    ttos_spinlock_t lock;          /* 位图锁 */
};

struct major_map g_cmajor_map, g_bmajor_map;

/**
 * @brief 锁定设备号管理系统
 */
static void lock_dev_t(struct major_map *map, irq_flags_t *flags)
{
    spin_lock_irqsave(&map->lock, *flags);
}

/**
 * @brief 解锁设备号管理系统
 */
static void unlock_dev_t(struct major_map *map, irq_flags_t *flags)
{
    spin_unlock_irqrestore(&map->lock, *flags);
}

/**
 * @brief 设置位图中的位
 *
 * @param bitmap 位图
 * @param bit 位索引
 */
static void set_bit(unsigned long *bitmap, unsigned int bit)
{
    bitmap[bit / (sizeof(unsigned long) * 8)] |= (1UL << (bit % (sizeof(unsigned long) * 8)));
}

/**
 * @brief 清除位图中的位
 *
 * @param bitmap 位图
 * @param bit 位索引
 */
static void clear_bit(unsigned long *bitmap, unsigned int bit)
{
    bitmap[bit / (sizeof(unsigned long) * 8)] &= ~(1UL << (bit % (sizeof(unsigned long) * 8)));
}

/**
 * @brief 测试位图中的位
 *
 * @param bitmap 位图
 * @param bit 位索引
 * @return 位值（0或1）
 */
static int test_bit(unsigned long *bitmap, unsigned int bit)
{
    return (bitmap[bit / (sizeof(unsigned long) * 8)] >> (bit % (sizeof(unsigned long) * 8))) & 1;
}

/**
 * @brief 查找主设备号位图
 *
 * @param major 主设备号
 * @return 成功返回位图索引，失败返回-1
 */
static int find_major_bitmap(struct major_map *map, unsigned int major)
{
    for (unsigned int i = 0; i < map->count; i++)
    {
        if (map->major_bitmaps[i].major == major)
        {
            return i;
        }
    }
    return -1;
}

/**
 * @brief 分配主设备号位图
 *
 * @param major 主设备号
 * @param name 设备名称
 * @return 成功返回位图索引，失败返回-1
 */
static int alloc_major_bitmap(struct major_map *map, unsigned int major, const char *name)
{
    /* 检查容量 */
    if (map->count >= map->capacity)
    {
        unsigned int new_capacity = map->capacity * 2;
        major_bitmap_t *new_bitmaps =
            realloc(map->major_bitmaps, new_capacity * sizeof(major_bitmap_t));
        if (!new_bitmaps)
        {
            return -1;
        }
        map->major_bitmaps = new_bitmaps;
        map->capacity = new_capacity;
    }

    /* 分配位图 */
    unsigned int bitmap_size =
        (MAX_MINOR + 1 + (sizeof(unsigned long) * 8 - 1)) / (sizeof(unsigned long) * 8);
    unsigned long *bitmap = calloc(bitmap_size, sizeof(unsigned long));
    if (!bitmap)
    {
        return -1;
    }

    /* 初始化位图 */
    map->major_bitmaps[map->count].major = major;
    map->major_bitmaps[map->count].bitmap = bitmap;
    strncpy(map->major_bitmaps[map->count].name, name, 31);
    map->major_bitmaps[map->count].name[31] = '\0';

    return map->count++;
}

/**
 * @brief 释放主设备号位图
 *
 * @param idx 位图索引
 */
static void free_major_bitmap(struct major_map *map, int idx)
{
    if (idx < 0 || idx >= (int)map->count)
    {
        return;
    }

    free(map->major_bitmaps[idx].bitmap);

    /* 移动后面的位图 */
    if (idx < (int)map->count - 1)
    {
        memmove(&map->major_bitmaps[idx], &map->major_bitmaps[idx + 1],
                (map->count - idx - 1) * sizeof(major_bitmap_t));
    }

    map->count--;
}

/**
 * @brief 查找空闲主设备号
 *
 * @return 成功返回主设备号，失败返回0
 */
static unsigned int find_free_major(struct major_map *map)
{
    /* 从1开始，0保留 */
    for (unsigned int major = 1; major <= MAX_MAJOR; major++)
    {
        if (find_major_bitmap(map, major) < 0)
        {
            printk("alloc: %d\n", major);
            return major;
        }
    }
    return 0;
}

static int register_region(struct major_map *map, dev_t from, int count, const char *name)
{
    unsigned int major = MAJOR(from);
    unsigned int minor = MINOR(from);
    irq_flags_t flags;

    /* 参数检查 */
    if (major > MAX_MAJOR || minor > MAX_MINOR || minor + count > MAX_MINOR + 1 || !name)
    {
        return -1;
    }

    lock_dev_t(map, &flags);

    int idx = find_major_bitmap(map, major);
    if (idx < 0)
    {
        /* 主设备号不存在，分配新的 */
        idx = alloc_major_bitmap(map, major, name);
        if (idx < 0)
        {
            unlock_dev_t(map, &flags);
            return -1;
        }
    }
    else
    {
        /* 主设备号已存在，检查次设备号是否可用 */
        for (unsigned int i = 0; i < count; i++)
        {
            if (test_bit(map->major_bitmaps[idx].bitmap, minor + i))
            {
                unlock_dev_t(map, &flags);
                return -1;
            }
        }
    }

    /* 标记次设备号为已使用 */
    for (unsigned int i = 0; i < count; i++)
    {
        set_bit(map->major_bitmaps[idx].bitmap, minor + i);
    }

    unlock_dev_t(map, &flags);
    return major;
}

int register_chrdev_region(dev_t from, int count, const char *name)
{
    return register_region(&g_cmajor_map, from, count, name);
}

int register_blkdev_region(dev_t from, int count, const char *name)
{
    return register_region(&g_bmajor_map, from, count, name);
}

static int alloc_region(struct major_map *map, dev_t *dev, unsigned baseminor, int count, const char *name)
{
    irq_flags_t flags;
    /* 参数检查 */
    if (!dev || baseminor > MAX_MINOR || baseminor + count > MAX_MINOR + 1 || !name)
    {
        return -1;
    }

    lock_dev_t(map, &flags);

    /* 动态分配主设备号 */
    unsigned int major = find_free_major(map);
    if (major == 0)
    {
        unlock_dev_t(map, &flags);
        return -1;
    }

    /* 注册设备号区域 */
    int ret = register_region(map, MKDEV(major, baseminor), count, name);
    if (ret < 0)
    {
        unlock_dev_t(map, &flags);
        return ret;
    }

    *dev = MKDEV(major, baseminor);
    unlock_dev_t(map, &flags);
    return 0;
}

int alloc_chrdev_region(dev_t *dev, int baseminor, int count, const char *name)
{
    return alloc_region(&g_cmajor_map, dev, baseminor, count, name);
}

int alloc_blkdev_region(dev_t *dev, int baseminor, int count, const char *name)
{
    return alloc_region(&g_bmajor_map, dev, baseminor, count, name);
}

static void unregister_region(struct major_map *map, dev_t from, int count)
{
    irq_flags_t flags;
    unsigned int major = MAJOR(from);
    unsigned int minor = MINOR(from);

    /* 参数检查 */
    if (major > MAX_MAJOR || minor > MAX_MINOR || minor + count > MAX_MINOR + 1)
    {
        return;
    }

    lock_dev_t(map, &flags);

    int idx = find_major_bitmap(map, major);
    if (idx < 0)
    {
        unlock_dev_t(map, &flags);
        return;
    }

    /* 清除次设备号标记 */
    for (unsigned int i = 0; i < count; i++)
    {
        clear_bit(g_cmajor_map.major_bitmaps[idx].bitmap, minor + i);
    }

    /* 检查是否所有次设备号都未使用 */
    bool all_free = true;
    for (unsigned int i = 0; i <= MAX_MINOR; i++)
    {
        if (test_bit(g_cmajor_map.major_bitmaps[idx].bitmap, i))
        {
            all_free = false;
            break;
        }
    }

    /* 如果所有次设备号都未使用，释放主设备号 */
    if (all_free)
    {
        free_major_bitmap(map, idx);
    }

    unlock_dev_t(map, &flags);
}

void unregister_chrdev_region(dev_t from, int count)
{
    return unregister_region(&g_cmajor_map, from, count);
}

void unregister_blkdev_region(dev_t from, int count)
{
    return unregister_region(&g_bmajor_map, from, count);
}


static int init_major_map(struct major_map *map)
{
    map->capacity = 16;
    map->count = 0;
    map->major_bitmaps = calloc(map->capacity, sizeof(major_bitmap_t));
    if (!map->major_bitmaps)
    {
        return -ENOMEM;
    }
    spin_lock_init(&map->lock);
    return 0;
}

static int destroy_major_map(struct major_map *map)
{
    for (unsigned int i = 0; i < map->count; i++)
    {
        free(map->major_bitmaps[i].bitmap);
    }
    free(map->major_bitmaps);
    map->major_bitmaps = NULL;
    return 0;
}

int dev_t_init(void)
{
    int ret = 0;
    ret = init_major_map(&g_cmajor_map);
    if (ret < 0)
    {
        return ret;
    }
    ret = init_major_map(&g_bmajor_map);
    if (ret < 0)
    {
        destroy_major_map(&g_cmajor_map);
        return ret;
    }

    return 0;
}
INIT_EXPORT_PRE_DRIVER(dev_t_init, "dev_t init");
