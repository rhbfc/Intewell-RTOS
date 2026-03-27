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
 * @file dev_t.h
 * @brief 设备号管理系统头文件
 *
 * 提供设备号(dev_t)的申请、释放和管理功能
 */

#ifndef _DEV_T_H
#define _DEV_T_H

#include <stdbool.h>
#include <stdint.h>

#define __NEED_dev_t
#include <bits/alltypes.h>

#define MAX_MAJOR 0xff

#define MAX_MINOR 512

/**
 * @brief 从dev_t中提取主设备号
 *
 * @param x 设备号
 * @return 主设备号
 */
#define MAJOR(x) ((unsigned)((((x) >> 31 >> 1) & 0xfffff000) | (((x) >> 8) & 0x00000fff)))

/**
 * @brief 从dev_t中提取次设备号
 *
 * @param x 设备号
 * @return 次设备号
 */
#define MINOR(x) ((unsigned)((((x) >> 12) & 0xffffff00) | ((x)&0x000000ff)))

/**
 * @brief 根据主设备号和次设备号生成dev_t
 *
 * @param x 主设备号
 * @param y 次设备号
 * @return 设备号(dev_t)
 */
#define MKDEV(x, y)                                                                                \
    ((((x)&0xfffff000ULL) << 32) | (((x)&0x00000fffULL) << 8) | (((y)&0xffffff00ULL) << 12) |      \
     (((y)&0x000000ffULL)))

/**
 * @brief 申请一个char设备号范围
 *
 * @param from 起始次设备号
 * @param count 申请的设备数量
 * @param name 设备名称
 * @return 成功返回主设备号，失败返回负数错误码
 */
int register_chrdev_region(dev_t from, int count, const char *name);

/**
 * @brief 申请一个block设备号范围
 *
 * @param from 起始次设备号
 * @param count 申请的设备数量
 * @param name 设备名称
 * @return 成功返回主设备号，失败返回负数错误码
 */
int register_blkdev_region(dev_t from, int count, const char *name);

/**
 * @brief 动态申请char设备号范围
 *
 * @param major 指定的主设备号，0表示动态分配
 * @param baseminor 起始次设备号
 * @param count 申请的设备数量
 * @param name 设备名称
 * @param dev 返回分配的第一个设备号
 * @return 成功返回0，失败返回负数错误码
 */
int alloc_chrdev_region(dev_t *dev, int baseminor, int count, const char *name);

/**
 * @brief 动态申请block设备号范围
 *
 * @param major 指定的主设备号，0表示动态分配
 * @param baseminor 起始次设备号
 * @param count 申请的设备数量
 * @param name 设备名称
 * @param dev 返回分配的第一个设备号
 * @return 成功返回0，失败返回负数错误码
 */
int alloc_blkdev_region(dev_t *dev, int baseminor, int count, const char *name);

/**
 * @brief 释放char设备号范围
 *
 * @param from 起始设备号
 * @param count 设备数量
 */
void unregister_chrdev_region(dev_t from, int count);

/**
 * @brief 释放block设备号范围
 *
 * @param from 起始设备号
 * @param count 设备数量
 */
void unregister_blkdev_region(dev_t from, int count);

#endif /* _CHAR_DEV_T_H */
