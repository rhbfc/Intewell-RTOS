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

#ifndef __NETDEV_SOCK_IOCTL__
#define __NETDEV_SOCK_IOCTL__

#include <net/route.h>

/* Non-Standard IO CTL Commands */

#define SIOCXPCAP 0xff0001
#define SIOCGIFALLIPS 0xff0002
#define SIOCAIFADDR 0xff0003

typedef struct netif_ips
{
    char name[IFNAMSIZ];
    int ip_count;
    struct
    {
        struct in_addr addr;
        struct in_addr netmask;
        uint32_t flags;
    } * addrs;
} netif_ips_t;

extern int ndev_ioctl_get_ifconf(struct ifconf *ifc);
extern int ndev_ioctl_get_mtu_by_name(struct ifreq *ifr);
extern int ndev_ioctl_get_hwaddr_by_name(struct ifreq *ifr);
extern int ndev_ioctl_get_flags_by_name(struct ifreq *ifr);
extern int ndev_ioctl_get_ip_by_name(struct ifreq *ifr);
extern int ndev_ioctl_get_bcast_by_name(struct ifreq *ifr);
extern int ndev_ioctl_get_nmask_by_name(struct ifreq *ifr);
extern int ndev_ioctl_get_gw_by_name(struct ifreq *ifr);

extern int ndev_ioctl_set_hwaddr_by_name(struct ifreq *ifr);
extern int ndev_ioctl_set_mtu_by_name(struct ifreq *ifr);
extern int ndev_ioctl_set_flags_by_name(struct ifreq *ifr);
extern int ndev_ioctl_set_ip_by_name(struct ifreq *ifr);
extern int ndev_ioctl_set_nmask_by_name(struct ifreq *ifr);
extern int ndev_ioctl_set_gw_by_name(struct ifreq *ifr);

extern int ndev_ioctl_index_to_name(struct ifreq *ifr);
extern int ndev_ioctl_name_to_index(struct ifreq *ifr);

extern int ndev_ioctl_add_route(struct rtentry *rte);
extern int ndev_ioctl_del_route(struct rtentry *rte);

extern int ndev_ioctl_get_if_all_ips(struct netif_ips *nip);
extern int ndev_ioctl_add_ip_by_name(struct ifreq *ifr);
extern int ndev_ioctl_del_ip_by_name(struct ifreq *ifr);

extern int ndev_ioctl_read_mii (struct ifreq *ifr);
extern int ndev_ioctl_write_mii (struct ifreq *ifr);

extern int ndev_ioctl_get_afpacket_stats (struct ifreq *ifr);

#endif
