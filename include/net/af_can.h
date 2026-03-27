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

#ifndef __INCLUDE_NET_AF_CAN_H__
#define __INCLUDE_NET_AF_CAN_H__
#include <driver/can/can.h>

typedef struct can_frame CAN_MSG;
typedef struct canfd_frame CAN_FDMSG;
typedef struct can_dev CAN_DEV;

typedef enum
{
    CAN_FRAME_STD = 0,
    CAN_FRAME_FD
} CAN_FRAME_TYPE;

typedef struct can_sock
{
    struct list_node node;
    uint16_t proto;
    CAN_DEV *candev;
    int fs_fd;
    struct file *filep;
    sockopt_t sock_opts;
} can_sock_t;

struct active_can_sockets
{
    uint32_t count;
    uint32_t max_count;

    /* 存放可用已激活sockets指针的堆上数组 */
    can_sock_t **socks;
};

extern ttos_spinlock_t ACTIVE_CAN_SOCKETS_LOCK;
extern struct active_can_sockets ACTIVE_CAN_SOCKETS;

#endif /* __INCLUDE_NET_AF_CAN_H__ */