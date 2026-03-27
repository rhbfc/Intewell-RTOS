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

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stddef.h>
#include <string.h>
#include <system/kconfig.h>
#include <net/netdev.h>
#include <net/if.h>
#include <net/can_dev.h>
#include <symtab.h>

#undef KLOG_TAG
#define KLOG_TAG "CAN_DEV"
#include <klog.h>

#define CONFIG_CAN_RX_TASK_PRIORITY 30
#define CONFIG_CAN_RX_TASK_STACK_SIZE 4096

/* 系统内CAN设备总数 */
static int can_dev_count;
LIST_HEAD(CAN_DEV_LIST);
DEFINE_SPINLOCK(CAN_DEV_LOCK);

extern const struct netdev_ops can_netdev_ops;

int can_device_init(CAN_DEV_INFO *dev_info, void *dev_ctrl, struct device *device)
{
    CAN_DEV *candev = NULL;
    NET_DEV *netdev = NULL;
    int ret;
    long can_flags;

    if ((dev_info == NULL) || (dev_info->dev_name == NULL) || (dev_info->can_func == NULL) || (dev_ctrl == NULL) || (device == NULL))
    {
        return -ERROR;
    }

    candev = calloc(1, sizeof(CAN_DEV));
    if (candev == NULL)
    {
        KLOG_E("CAN Device Allocate Memory Failed");
        return -ERROR;
    }

    candev->dev_ctrl = dev_ctrl;
    candev->can_unit = can_dev_count++;
    candev->dev_info = dev_info;
    candev->device = device;

    //TODO: 添加CAN设备注册时的回调函数
    netdev = netdev_install_dev(candev, NET_LL_CAN, &can_netdev_ops, NULL, NULL);
    if (netdev == NULL)
    {
        KLOG_E("Install netdev failed");

        goto err_can_dev_init;
    }

    candev->netdev = netdev;

    /* 将candev加入链表 */
    spin_lock_irqsave(&CAN_DEV_LOCK, can_flags);
    list_add_tail(&candev->node, &CAN_DEV_LIST);
    spin_unlock_irqrestore(&CAN_DEV_LOCK, can_flags);

    candev->init_flag = 0xfeedcafe;

    netdev_set_up(netdev);

    return OK;

err_can_dev_init:
    free(candev);
    return -ERROR;
}

/* 应通过netdev_uninstall_dev()调用 */
int can_device_deinit(CAN_DEV *candev)
{
    long can_flags;

    if (candev == NULL || candev->init_flag != 0xfeedcafe)
    {
        return -ERROR;
    }

    /* 从CAN设备链表移除 */
    spin_lock_irqsave(&CAN_DEV_LOCK, can_flags);
    list_delete(&candev->node);
    spin_unlock_irqrestore(&CAN_DEV_LOCK, can_flags);

    can_dev_count--;

    /* 释放CAN_DEV结构体内存（与ETH_DEV不同，CAN_DEV是动态分配的） */
    free(candev);

    return OK;
}