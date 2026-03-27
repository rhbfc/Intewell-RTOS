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
#include <net/if.h>
#include <net/netdev.h>
#include <net/netdev_ioctl.h>
#include <net/can_dev.h>
#include <shell.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

static int can_netdev_set_up(NET_DEV *netdev)
{
    CAN_DEV *candev = netdev->link_data;
    int i;

    netdev->flags |= NETDEV_FLAG_STATUS;

    return OK;
}

static int can_netdev_set_down(NET_DEV *netdev)
{
    CAN_DEV *candev = netdev->link_data;
    
    netdev->flags &= ~NETDEV_FLAG_STATUS;

    return OK;
}

const struct netdev_ops can_netdev_ops =
{
    .set_up = can_netdev_set_up,
    .set_down = can_netdev_set_down,
    .set_link_up = NULL,
    .set_link_down = NULL,
    .set_mac = NULL,
    .set_flag = NULL,
    .set_addr_info = NULL,
    .set_mtu = NULL,
    .set_dhcp = NULL,
    .set_default = NULL
};