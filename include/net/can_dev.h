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

#ifndef __INCLUDE_NET_CAN_DEV_H__
#define __INCLUDE_NET_CAN_DEV_H__

#include <system/kconfig.h>
#include <spinlock.h>
#include <wqueue.h>
#include <fs/fs.h>
#include <net/net.h>
#include <netinet/in.h>
#include <list.h>
#include <net/netdev.h>
#include <net/af_can.h>
#include <driver/device.h>
#include <driver/can/can.h>

#define CAN_HDRLEN    8 /* 4+1+1+1+1字节 */

typedef struct can_dev_funcs
{
    int (*can_ifup)(struct file *filep);
    int (*can_ifdown)(struct file *filep);
    int (*can_ioctl)(struct file *filep, unsigned int cmd, unsigned long arg);
    ssize_t (*can_send)(struct file *filep, const char *buffer, size_t buflen);
    ssize_t (*can_recv)(struct file *filep, char *buffer, size_t buflen);
    int (*can_poll)(struct file *filep, struct kpollfd *fds, bool setup);
    int (*can_close)(struct file *filep);
} CAN_DEV_FUNCS;

typedef struct can_dev_info
{
    struct list_node node;
    char *dev_name;           /* 设备节点名 */
    CAN_DEV_FUNCS *can_func;  /* 功能表 */
} CAN_DEV_INFO;

typedef struct can_dev
{
    struct list_node node;
    NET_DEV *netdev;            /* netdev指针 */
    struct device *device;
    int can_unit;               /* CAN设备单元号 */
    int init_flag;

    CAN_DEV_INFO *dev_info;     /* CAN设备信息 */

    void *dev_ctrl; /* CAN设备驱动管理结构体 */
} CAN_DEV;

extern int can_device_init(CAN_DEV_INFO *dev_info, void *dev_ctrl, struct device *device);
extern int can_device_deinit(CAN_DEV *candev);

#endif /* __INCLUDE_NET_CAN_DEV_H__ */