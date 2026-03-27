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
#include <net/ethernet_dev.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/net.h>
#include <net/netdev.h>
#include <net/netdev_ioctl.h>
#include <net/netdev_route.h>
#include <net/route.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/ioctl.h>

extern ttos_spinlock_t netdev_spinlock;
/* Provide this function in your network stack */
extern void impl_ethernet_get_all_ips(ETH_DEV *eth, netif_ips_t *ips);
extern int impl_lwip_netdev_add_addr(NET_DEV *netdev, struct in_addr *ip_addr,
                                     struct in_addr *netmask);
extern int impl_lwip_netdev_del_addr(NET_DEV *netdev, struct in_addr *ip_addr,
                                     struct in_addr *netmask);

#define FLAG_SET_CHECK(exp, err)                                                                   \
    do                                                                                             \
    {                                                                                              \
        err = (exp);                                                                               \
    } while (0)

static void ndev_get_ifr_flags(NET_DEV *ndev, short int *uflags)
{
    *uflags = netdev_flags_to_posix_flags(ndev);
}

static void ndev_get_if_ip(NET_DEV *ndev, struct sockaddr *addr)
{
    struct sockaddr_in *saddr = (struct sockaddr_in *)addr;
    saddr->sin_addr = ndev->ll_info.ethernet.ip_addr;
}

static void ndev_get_if_nmask(NET_DEV *ndev, struct sockaddr *addr)
{
    struct sockaddr_in *saddr = (struct sockaddr_in *)addr;
    saddr->sin_addr = ndev->ll_info.ethernet.netmask;
}

static void ndev_get_if_bcast(NET_DEV *ndev, struct sockaddr *addr)
{
    struct sockaddr_in *saddr = (struct sockaddr_in *)addr;
    struct in_addr baddr;

    baddr.s_addr = ndev->ll_info.ethernet.ip_addr.s_addr | (~ndev->ll_info.ethernet.netmask.s_addr);
    saddr->sin_addr = baddr;
}

static void ndev_get_if_hwaddr(NET_DEV *ndev, struct sockaddr *addr)
{
    switch (ndev->ll_type)
    {
    case NET_LL_ETHERNET:
        addr->sa_family = ARPHRD_ETHER;
        memcpy(addr->sa_data, &ndev->hwaddr, ndev->hwaddr_len);
        break;

    case NET_LL_CAN:
        addr->sa_family = ARPHRD_CAN;
        memset(addr->sa_data, 0, sizeof(addr->sa_data));
        break;

    default:
        addr->sa_family = ARPHRD_VOID;
        break;
    }
}

static void netdev_get_all_ifconf(struct ifconf *ifc)
{
    long spin_flag;
    NET_DEV *ndev = NULL;
    struct ifreq *ifr = NULL;
    struct list_node *node;
    int ndev_count = 0;

    const int devs_max = ifc->ifc_len / sizeof(struct ifreq);

    spin_lock_irqsave(&netdev_spinlock, spin_flag);

    for (node = &NETDEV_LIST->node; node != NULL; node = list_next(node, &(NETDEV_LIST->node)))
    {
        ndev = list_entry(node, NET_DEV, node);

        ifr = &(ifc->ifc_req[ndev_count]);

        snprintf(ifr->ifr_name, IF_NAMESIZE, "%s", ndev->name);

        /* IP */
        ndev_get_if_ip(ndev, &ifr->ifr_addr);

        ndev_count += 1;
        if (ndev_count >= devs_max)
        {
            break;
        }
    }

    spin_unlock_irqrestore(&netdev_spinlock, spin_flag);

    ifc->ifc_len = ndev_count * sizeof(struct ifreq);
}

int ndev_ioctl_get_ifconf(struct ifconf *ifc)
{
    netdev_get_all_ifconf(ifc);

    return 0;
}

int ndev_ioctl_get_mtu_by_name(struct ifreq *ifr)
{
    NET_DEV *ndev = NULL;

    ndev = netdev_get_by_name(ifr->ifr_name);

    if (ndev == NULL)
    {
        return -ENODEV;
    }

    ifr->ifr_mtu = (int)ndev->ll_info.ethernet.mtu;

    return 0;
}

int ndev_ioctl_get_hwaddr_by_name(struct ifreq *ifr)
{
    NET_DEV *ndev = NULL;

    ndev = netdev_get_by_name(ifr->ifr_name);

    if (ndev == NULL)
    {
        return -ENODEV;
    }

    ndev_get_if_hwaddr(ndev, &ifr->ifr_hwaddr);

    return 0;
}

int ndev_ioctl_get_flags_by_name(struct ifreq *ifr)
{
    NET_DEV *ndev = NULL;

    ndev = netdev_get_by_name(ifr->ifr_name);
    if (ndev == NULL)
    {
        return -ENODEV;
    }

    ndev_get_ifr_flags(ndev, &ifr->ifr_flags);

    return 0;
}

int ndev_ioctl_get_ip_by_name(struct ifreq *ifr)
{
    NET_DEV *ndev = NULL;

    ndev = netdev_get_by_name(ifr->ifr_name);

    if (ndev == NULL)
    {
        return -ENODEV;
    }

    if (AF_INET != ifr->ifr_netmask.sa_family)
    {
        return -EINVAL;
    }

    ndev_get_if_ip(ndev, &ifr->ifr_addr);

    return 0;
}

int ndev_ioctl_get_bcast_by_name(struct ifreq *ifr)
{
    NET_DEV *ndev = NULL;

    ndev = netdev_get_by_name(ifr->ifr_name);

    if (ndev == NULL)
    {
        return -ENODEV;
    }

    if (AF_INET != ifr->ifr_netmask.sa_family)
    {
        return -EINVAL;
    }

    ndev_get_if_bcast(ndev, &ifr->ifr_broadaddr);

    return 0;
}

int ndev_ioctl_get_nmask_by_name(struct ifreq *ifr)
{
    NET_DEV *ndev = NULL;

    ndev = netdev_get_by_name(ifr->ifr_name);

    if (ndev == NULL)
    {
        return -ENODEV;
    }

    if (AF_INET != ifr->ifr_netmask.sa_family)
    {
        return -EINVAL;
    }

    ndev_get_if_nmask(ndev, &ifr->ifr_netmask);

    return 0;
}

int ndev_ioctl_set_hwaddr_by_name(struct ifreq *ifr)
{
    NET_DEV *ndev = NULL;

    ndev = netdev_get_by_name(ifr->ifr_name);

    if (ndev == NULL)
    {
        return -ENODEV;
    }

    return netdev_set_mac(ndev, (unsigned long)ifr);
}

int ndev_ioctl_set_mtu_by_name(struct ifreq *ifr)
{
    NET_DEV *ndev = NULL;

    ndev = netdev_get_by_name(ifr->ifr_name);

    if (ndev == NULL)
    {
        return -ENODEV;
    }

    ndev->ll_info.ethernet.mtu = ifr->ifr_mtu;

    return 0;
}

int ndev_ioctl_set_flags_by_name(struct ifreq *ifr)
{
    short int flags = ifr->ifr_flags;

    NET_DEV *ndev = NULL;

    ndev = netdev_get_by_name(ifr->ifr_name);

    int err = OK;

    if (ndev == NULL)
    {
        return -ENODEV;
    }

    if (flags & IFF_UP)
    {
        FLAG_SET_CHECK(netdev_set_up(ndev), err);
    }
    else
    {
        FLAG_SET_CHECK(netdev_set_down(ndev), err);
    }

    if (flags & IFF_NOARP)
    {
        FLAG_SET_CHECK(ndev->ops->set_flag(ndev, NETDEV_FLAG_ETHARP, NETDEV_FLAG_UNSET), err);
    }
    else
    {
        FLAG_SET_CHECK(ndev->ops->set_flag(ndev, NETDEV_FLAG_ETHARP, NETDEV_FLAG_SET), err);
    }

    if (flags & IFF_BROADCAST)
    {
        FLAG_SET_CHECK(ndev->ops->set_flag(ndev, NETDEV_FLAG_BROADCAST, NETDEV_FLAG_SET), err);
    }
    else
    {
        FLAG_SET_CHECK(ndev->ops->set_flag(ndev, NETDEV_FLAG_BROADCAST, NETDEV_FLAG_UNSET), err);
    }

    if (flags & IFF_MULTICAST)
    {
        FLAG_SET_CHECK(ndev->ops->set_flag(ndev, NETDEV_FLAG_IGMP, NETDEV_FLAG_SET), err);
    }
    else
    {
        FLAG_SET_CHECK(ndev->ops->set_flag(ndev, NETDEV_FLAG_IGMP, NETDEV_FLAG_UNSET), err);
    }

    return err;
}

int ndev_ioctl_set_ip_by_name(struct ifreq *ifr)
{
    NET_DEV *ndev = NULL;
    struct sockaddr_in *addr = (struct sockaddr_in *)&ifr->ifr_addr;
    int ret;

    ndev = netdev_get_by_name(ifr->ifr_name);

    if (ndev == NULL)
    {
        return -ENODEV;
    }

    if (addr->sin_family != AF_INET)
    {
        return -EINVAL;
    }

    ret = netdev_set_ipaddr(ndev, (const struct in_addr *)&addr->sin_addr);

    if (ret == OK && addr->sin_addr.s_addr == 0)
    {
        (void)netdev_set_dhcp(ndev, true);
    }

#ifdef CONFIG_ARP_IP_ADDR_CHANGE
    if (netdev_is_link_up(ndev))
    {
        eth_arp_announce(NETDEV_TO_ETHDEV(ndev));
    }
#endif

    return ret;
}

int ndev_ioctl_set_nmask_by_name(struct ifreq *ifr)
{
    NET_DEV *ndev = NULL;
    struct sockaddr_in *addr = (struct sockaddr_in *)&ifr->ifr_netmask;

    ndev = netdev_get_by_name(ifr->ifr_name);

    if (ndev == NULL)
    {
        return -ENODEV;
    }

    if (addr->sin_family != AF_INET)
    {
        return -EINVAL;
    }

    return netdev_set_netmask(ndev, (const struct in_addr *)&addr->sin_addr);
}

int ndev_ioctl_name_to_index(struct ifreq *ifr)
{
    NET_DEV *ndev = NULL;

    ndev = netdev_get_by_name(ifr->ifr_name);

    if (ndev == NULL)
    {
        return -ENODEV;
    }

    ifr->ifr_ifindex = ndev->netdev_unit;

    return OK;
}

int ndev_ioctl_index_to_name(struct ifreq *ifr)
{
    int index = ifr->ifr_ifindex;

    NET_DEV *ndev = netdev_get_by_unit(index);

    if (NULL == ndev)
    {
        return -ENODEV;
    }

    snprintf(ifr->ifr_name, sizeof(ifr->ifr_name), "%s", ndev->name);

    return OK;
}

int ndev_ioctl_add_route(struct rtentry *rte)
{
    bool is_default;

    if (rte->rt_dst.sa_family != AF_INET || rte->rt_gateway.sa_family != AF_INET)
    {
        return -ENOTSUP;
    }

    struct sockaddr_in *dst = (struct sockaddr_in *)&rte->rt_dst;
    struct sockaddr_in *gw = (struct sockaddr_in *)&rte->rt_gateway;
    struct sockaddr_in *mask = (struct sockaddr_in *)&rte->rt_genmask;

    if (NULL == rte->rt_dev)
    {
        return -ENETUNREACH;
    }

    NET_DEV *ndev = netdev_get_by_name(rte->rt_dev);

    if (NULL == ndev)
    {
        return -ENODEV;
    }

    is_default = (dst->sin_addr.s_addr == 0) && ((rte->rt_flags & RTF_GATEWAY) == RTF_GATEWAY);

    /* TODO 判断是不是设置默认路由等 */
    return route_list_add(ndev, dst, gw, mask, (uint32_t)rte->rt_flags, is_default);
}

int ndev_ioctl_del_route(struct rtentry *rte)
{
    if (rte->rt_dst.sa_family != AF_INET && rte->rt_gateway.sa_family != AF_INET)
    {
        return -ENOTSUP;
    }

    struct sockaddr_in *dst = (struct sockaddr_in *)&rte->rt_dst;
    struct sockaddr_in *gw = (struct sockaddr_in *)&rte->rt_gateway;
    struct sockaddr_in *mask = (struct sockaddr_in *)&rte->rt_genmask;

    NET_DEV *ndev = NULL;

    if (NULL != rte->rt_dev)
    {
        NET_DEV *ndev = netdev_get_by_name(rte->rt_dev);

        if (NULL == ndev)
        {
            return -ENODEV;
        }
    }

    /* TODO 判断是不是设置默认路由等 */
    return route_list_del(ndev, dst, gw, mask);
}

int ndev_ioctl_get_if_all_ips(struct netif_ips *nip)
{
    NET_DEV *ndev = NULL;

    ndev = netdev_get_by_name(nip->name);

    if (ndev == NULL)
    {
        return -ENODEV;
    }

#if CONFIG_LWIP_2_2_0
    impl_ethernet_get_all_ips(ndev->link_data, nip);

    return OK;
#else
    return -ENOTSUP;
#endif
}

int ndev_ioctl_add_ip_by_name(struct ifreq *ifr)
{
    NET_DEV *ndev = NULL;
    struct in_addr *ip = (struct in_addr *)&ifr[0].ifr_addr;
    struct in_addr *netmask = (struct in_addr *)&ifr[1].ifr_netmask;
    int ret;

    ndev = netdev_get_by_name(ifr[0].ifr_name);

    if (ndev == NULL)
    {
        return -ENODEV;
    }

    if (ip->s_addr == 0)
    {
        return -EINVAL;
    }

#if CONFIG_LWIP_2_2_0

    ret = impl_lwip_netdev_add_addr(ndev, ip, netmask->s_addr == 0 ? NULL : netmask);

#ifdef CONFIG_ARP_IP_ADDR_CHANGE
    if (netdev_is_link_up(ndev))
    {
        eth_arp_announce(NETDEV_TO_ETHDEV(ndev));
    }
#endif

    return ret;
#else
    return -ENOTSUP;
#endif
}

int ndev_ioctl_del_ip_by_name(struct ifreq *ifr)
{
    NET_DEV *ndev = NULL;
    struct in_addr *ip = (struct in_addr *)&ifr[0].ifr_addr;
    struct in_addr *netmask = (struct in_addr *)&ifr[1].ifr_netmask;

    ndev = netdev_get_by_name(ifr[0].ifr_name);

    if (ndev == NULL)
    {
        return -ENODEV;
    }

#if CONFIG_LWIP_2_2_0
    return impl_lwip_netdev_del_addr(ndev, ip, netmask->s_addr == 0 ? NULL : netmask);
#else
    return -ENOTSUP;
#endif
}

int ndev_ioctl_read_mii(struct ifreq *ifr)
{
    NET_DEV *netdev = NULL;
    ETH_DEV *ethdev = NULL;
    int ret;

    netdev = netdev_get_by_name(ifr->ifr_name);
    if (netdev == NULL)
    {
        return -ENODEV;
    }

    ethdev = netdev->link_data;

    if (ethdev->drv_info->eth_func->ioctl == NULL)
    {
        return -ENOTSUP;
    }

    ret = ethdev->drv_info->eth_func->ioctl(ethdev, SIOCGMIIREG, (unsigned long)ifr);

    return ret;
}

int ndev_ioctl_write_mii(struct ifreq *ifr)
{
    NET_DEV *netdev = NULL;
    ETH_DEV *ethdev = NULL;
    int ret;

    netdev = netdev_get_by_name(ifr->ifr_name);
    if (netdev == NULL)
    {
        return -ENODEV;
    }

    ethdev = netdev->link_data;

    if (ethdev->drv_info->eth_func->ioctl == NULL)
    {
        return -ENOTSUP;
    }

    ret = ethdev->drv_info->eth_func->ioctl(ethdev, SIOCSMIIREG, (unsigned long)ifr);

    return ret;
}

int ndev_ioctl_get_afpacket_stats(struct ifreq *ifr)
{
    NET_DEV *netdev = NULL;
    AFPACKET_STATS *pkt_stats = NULL;

    netdev = netdev_get_by_name(ifr->ifr_name);
    if (netdev == NULL)
    {
        return -ENODEV;
    }

    pkt_stats = (AFPACKET_STATS *)ifr->ifr_data;

    if (pkt_stats != NULL)
    {
        pkt_stats->out_bytes = netdev->ll_info.ethernet.afpkt_stats.out_bytes;
        pkt_stats->out_pacs = netdev->ll_info.ethernet.afpkt_stats.out_pacs;
        pkt_stats->eout_bytes = netdev->ll_info.ethernet.afpkt_stats.eout_bytes;
        pkt_stats->eout_pacs = netdev->ll_info.ethernet.afpkt_stats.eout_pacs;
        pkt_stats->in_bytes = netdev->ll_info.ethernet.afpkt_stats.in_bytes;
        pkt_stats->in_pacs = netdev->ll_info.ethernet.afpkt_stats.in_pacs;
        pkt_stats->din_bytes = netdev->ll_info.ethernet.afpkt_stats.din_bytes;
        pkt_stats->din_pacs = netdev->ll_info.ethernet.afpkt_stats.din_pacs;

        return OK;
    }
    else
    {
        return -EINVAL;
    }
}
