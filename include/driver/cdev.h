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
 * @file char_dev.h
 * @brief 字符设备框架头文件
 *
 * 提供字符设备的注册、注销和操作接口
 */

#ifndef _CHAR_DEV_H
#define _CHAR_DEV_H

#include <driver/dev_t.h>
#include <stddef.h>
#include <stdint.h>
#include <list.h>

struct file_operations;

/**
 * @brief 字符设备结构体
 */
struct char_device
{
    dev_t dev;                         /* 设备号 */
    unsigned int count;                /* 设备数量 */
    const struct file_operations *ops; /* 文件操作函数表 */
    struct list_head list;             /* 设备链表节点 */
    void *private_data;                /* 私有数据 */
    char name[32];                     /* 设备名称 */
};

/**
 * @brief 注册字符设备驱动
 *
 * @param major 主设备号，0表示动态分配
 * @param name 设备名称
 * @param fops 文件操作函数表
 * @return 成功返回主设备号，失败返回负数错误码
 */
int register_chrdev(unsigned int major, const char *name, const struct file_operations *fops);

/**
 * @brief 注销字符设备驱动
 *
 * @param major 主设备号
 * @param name 设备名称
 */
void unregister_chrdev(unsigned int major, const char *name);


#endif /* _CHAR_DEV_H */
