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

#ifndef __NETDEV_ROUTE__
#define __NETDEV_ROUTE__

typedef struct static_route_item
{
    struct list_node node;
    NET_DEV *ndev;
    struct in_addr dest;
    struct in_addr gateway;
    struct in_addr netmask;
    uint32_t flags;
    bool is_default;
} static_route_item_t;

extern ttos_spinlock_t STATIC_ROUTE_LOCK;
extern struct list_node STATIC_ROUTE_LIST;

extern int route_list_add(NET_DEV *ndev, struct sockaddr_in *dst, struct sockaddr_in *gw, struct sockaddr_in *mask,
    uint32_t rt_flags, bool is_default);
extern int route_list_del(NET_DEV *ndev, struct sockaddr_in *dst, struct sockaddr_in *gw, struct sockaddr_in *mask);
extern NET_DEV *route_list_search (const struct in_addr const* dst, struct in_addr *netmask);
extern int clear_static_routes_for_dev(char *name);

#endif
