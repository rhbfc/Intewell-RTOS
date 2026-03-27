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

#include <arpa/inet.h>
#include <errno.h>
#include <net/ethernet_dev.h>
#include <net/can_dev.h>
#include <net/if.h>
#include <net/netdev.h>
#include <net/netdev_route.h>
#include <netinet/in.h>
#include <shell.h>
#include <spinlock.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#undef KLOG_TAG
#define KLOG_TAG "NETDEV"
#include <klog.h>

#define ip_addr_set_zero(ip)                                                                       \
    do                                                                                             \
    {                                                                                              \
        (ip)->s_addr = 0;                                                                          \
    } while (0)
#define IP_SET_TYPE_VAL(x, y)
#define ip_addr_cmp(ip1, ip2) ((ip1)->s_addr == (ip2)->s_addr)
#define ip_addr_copy(destip, srcip)                                                                \
    do                                                                                             \
    {                                                                                              \
        (destip)->s_addr = (srcip)->s_addr;                                                        \
    } while (0)

/* 系统默认网络设备 */
static NET_DEV *DEFAULT_NETDEV = NULL;

/* NetDev设备链表 */
NET_DEV *NETDEV_LIST = NULL;

NET_DEV *netdev_eth_lo = NULL;

/* 为了适配busybox，从1开始计数 */
static int netdev_count = 1;

static netdev_callback_fn g_netdev_register_callback = NULL;
static netdev_callback_fn g_netdev_default_change_callback = NULL;
DEFINE_SPINLOCK(netdev_spinlock);

static int netdev_register(NET_DEV *netdev, void *link_data, enum net_lltype link_type,
                           const struct netdev_ops *ops)
{
    ETH_DEV *ethdev = NULL;
    CAN_DEV *candev = NULL;
    unsigned short flags_mask;
    unsigned short index;
    irq_flags_t spin_flag;

    ASSERT(netdev);

    switch (link_type)
    {
    case (NET_LL_ETHERNET):
    {
        flags_mask =
            NETDEV_FLAG_STATUS | NETDEV_FLAG_LINK_STATUS | NETDEV_FLAG_INTERNET | NETDEV_FLAG_DHCP;
        netdev->flags &= ~flags_mask;

        ip_addr_set_zero(&(netdev->ll_info.ethernet.ip_addr));
        ip_addr_set_zero(&(netdev->ll_info.ethernet.netmask));
        ip_addr_set_zero(&(netdev->ll_info.ethernet.gw));

        IP_SET_TYPE_VAL(netdev->ll_info.ethernet.ip_addr, IPADDR_TYPE_V4);
        IP_SET_TYPE_VAL(netdev->ll_info.ethernet.netmask, IPADDR_TYPE_V4);
        IP_SET_TYPE_VAL(netdev->ll_info.ethernet.gw, IPADDR_TYPE_V4);
#if NETDEV_IPV6
        for (index = 0; index < NETDEV_IPV6_NUM_ADDRESSES; index++)
        {
            ip_addr_set_zero(&(netdev->ip6_addr[index]));
            IP_SET_TYPE_VAL(netdev->ip_addr, IPADDR_TYPE_V6);
        }
#endif /* NETDEV_IPV6 */
        netdev->status_callback = NULL;
        netdev->addr_callback = NULL;
        netdev->ll_type = link_type;
        netdev->ll_hdrlen = ETH_HDRLEN;

        netdev->ll_info.ethernet.pkt_stats.in_bytes = 0;
        netdev->ll_info.ethernet.pkt_stats.out_bytes = 0;
        netdev->ll_info.ethernet.pkt_stats.in_pacs = 0;
        netdev->ll_info.ethernet.pkt_stats.out_pacs = 0;
        netdev->ll_info.ethernet.pkt_stats.din_pacs = 0;
        netdev->ll_info.ethernet.pkt_stats.dout_pacs = 0;
        netdev->ll_info.ethernet.pkt_stats.ein_pacs = 0;
        netdev->ll_info.ethernet.pkt_stats.eout_pacs = 0;

        /* AF_PACKET统计信息复位 */
        atomic64_set(&(netdev->ll_info.ethernet.afpkt_stats.out_bytes), 0);
        atomic64_set(&(netdev->ll_info.ethernet.afpkt_stats.out_pacs), 0);
        atomic64_set(&(netdev->ll_info.ethernet.afpkt_stats.eout_bytes), 0);
        atomic64_set(&(netdev->ll_info.ethernet.afpkt_stats.eout_pacs), 0);
        atomic64_set(&(netdev->ll_info.ethernet.afpkt_stats.in_bytes), 0);
        atomic64_set(&(netdev->ll_info.ethernet.afpkt_stats.in_pacs), 0);
        atomic64_set(&(netdev->ll_info.ethernet.afpkt_stats.din_bytes), 0);
        atomic64_set(&(netdev->ll_info.ethernet.afpkt_stats.din_pacs), 0);

        ethdev = link_data;

        /*
          NETDEV中对以太网设备使用ethx的命名方式，但在LwIP的netif结构中仍使用驱动名(drv_info->drv_name)
        */
        snprintf(netdev->name, NETDEV_NAME_MAX, "eth%d", ethdev->eth_unit);

        ethdev->netdev = netdev;

        break;
    }

    case (NET_LL_LOOPBACK):
    {
        break;
    }

    case (NET_LL_SLIP):
    {
        break;
    }

    case (NET_LL_TUN):
    {
        break;
    }

    case (NET_LL_BLUETOOTH):
    {
        break;
    }

    case (NET_LL_IEEE80211):
    {
        break;
    }

    case (NET_LL_IEEE802154):
    {
        break;
    }

    case (NET_LL_PKTRADIO):
    {
        break;
    }

    case (NET_LL_MBIM):
    {
        break;
    }

    case (NET_LL_CAN):
    {
        flags_mask = NETDEV_FLAG_STATUS | NETDEV_FLAG_LINK_STATUS;
        netdev->flags &= ~flags_mask;

        netdev->ll_type = link_type;
        netdev->ll_hdrlen = CAN_HDRLEN;

        netdev->hwaddr_len = 0;
        memset(netdev->hwaddr, 0, NETDEV_HWADDR_MAX_LEN);

        netdev->status_callback = NULL;
        netdev->addr_callback = NULL;

        atomic64_set(&(netdev->ll_info.can.can_stats.out_frames), 0);
        atomic64_set(&(netdev->ll_info.can.can_stats.out_bytes), 0);
        atomic64_set(&(netdev->ll_info.can.can_stats.eout_frames), 0);
        atomic64_set(&(netdev->ll_info.can.can_stats.eout_bytes), 0);
        atomic64_set(&(netdev->ll_info.can.can_stats.in_frames), 0);
        atomic64_set(&(netdev->ll_info.can.can_stats.in_bytes), 0);
        atomic64_set(&(netdev->ll_info.can.can_stats.din_frames), 0);
        atomic64_set(&(netdev->ll_info.can.can_stats.din_bytes), 0);

        candev = link_data;

        snprintf(netdev->name, NETDEV_NAME_MAX, "can%d", candev->can_unit);

        candev->netdev = netdev;

        break;
    }

    case (NET_LL_CELL):
    {
        break;
    }
    }

    netdev->link_data = link_data; /* 例如(ETH_DEV *) */
    netdev->ops = ops;
    netdev->netdev_unit = netdev_count++;

    netdev->node = LIST_HEAD_INIT(netdev->node);

    spin_lock_irqsave(&netdev_spinlock, spin_flag);

    if (NULL == NETDEV_LIST)
    {
        NETDEV_LIST = netdev;
    }
    else
    {
        list_add_tail(&(netdev->node), &(NETDEV_LIST->node));
    }

    spin_unlock_irqrestore(&netdev_spinlock, spin_flag);

    if (g_netdev_register_callback)
    {
        g_netdev_register_callback(netdev, NETDEV_CB_REGISTER);
    }

    return OK;
}

static int netdev_unregister(NET_DEV *netdev)
{
    irq_flags_t spin_flag;
    struct list_node *node = NULL;
    struct list_node *next;
    NET_DEV *cur_netdev = NULL;

    ASSERT(netdev);

    spin_lock_irqsave(&netdev_spinlock, spin_flag);

    if (NULL == NETDEV_LIST)
    {
        spin_unlock_irqrestore(&netdev_spinlock, spin_flag);
        return -ERROR;
    }

    for (node = &(NETDEV_LIST->node); node; node = list_next(node, &(NETDEV_LIST->node)))
    {
        cur_netdev = list_entry(node, NET_DEV, node);
        if (cur_netdev == netdev)
        {
            if (NETDEV_LIST == netdev)
            {
                next = list_next(node, &(NETDEV_LIST->node));
                if (next)
                {
                    NETDEV_LIST = list_entry(next, NET_DEV, node);
                }
                else
                {
                    NETDEV_LIST = NULL;
                    DEFAULT_NETDEV = NULL;
                }
            }
            else
            {
                list_delete(&(cur_netdev->node));
            }

            spin_unlock_irqrestore(&netdev_spinlock, spin_flag);
            return OK;
        }
    }
    spin_unlock_irqrestore(&netdev_spinlock, spin_flag);

    if (NETDEV_LIST)
    {
        netdev_set_default(NETDEV_LIST);
    }

    return -ERROR;
}

/* link_data为链路层指针，如(ETH_DEV *) */
NET_DEV *netdev_install_dev(void *link_data, enum net_lltype link_type,
                            const struct netdev_ops *ops, netdev_install_cb callback,
                            void *cb_data)
{
    int result = 0;
    NET_DEV *netdev = NULL;

    if ((link_data == NULL) || (link_type > NET_LL_CELL))
    {
        return NULL;
    }

    netdev = (NET_DEV *)calloc(1, sizeof(NET_DEV));
    if (netdev == NULL)
    {
        return NULL;
    }

    result = netdev_register(netdev, link_data, link_type, ops);
    if (result != 0)
    {
        KLOG_E("netdev register failed");
        free(netdev);
        return NULL;
    }

    /* 使用第一张网卡作为默认网络设备 */
    if (netdev->ll_type == NET_LL_ETHERNET && DEFAULT_NETDEV == NULL)
    {
        netdev_set_default(netdev);
    }

    if (callback)
    {
        callback(netdev, cb_data);
    }

    return netdev;
}

int netdev_uninstall_dev(NET_DEV *netdev)
{
    int ret;

    ret = netdev_unregister(netdev);

    if (ret == OK)
    {
        switch (netdev->ll_type)
        {
        case (NET_LL_ETHERNET):
        {
            ret = eth_device_deinit(netdev->link_data);
            if (ret == OK)
            {
                free(netdev);
            }

            break;
        }

        case (NET_LL_CAN):
        {
            ret = can_device_deinit(netdev->link_data);
            if (ret == OK)
            {
                free(netdev);
            }

            break;
        }

        default:
            break;
        }
    }

    return ret;
}

void netdev_set_register_callback(netdev_callback_fn register_callback)
{
    g_netdev_register_callback = register_callback;
}

NET_DEV *netdev_get_first_by_flags(uint16_t flags)
{
    irq_flags_t spin_flag;
    struct list_node *node = NULL;
    NET_DEV *netdev = NULL;

    spin_lock_irqsave(&netdev_spinlock, spin_flag);

    if (NULL == NETDEV_LIST)
    {
        spin_unlock_irqrestore(&netdev_spinlock, spin_flag);
        return NULL;
    }

    for (node = &(NETDEV_LIST->node); node; node = list_next(node, &(NETDEV_LIST->node)))
    {
        netdev = list_entry(node, NET_DEV, node);
        if (netdev && (netdev->flags & flags) != 0)
        {
            spin_unlock_irqrestore(&netdev_spinlock, spin_flag);
            return netdev;
        }
    }

    spin_unlock_irqrestore(&netdev_spinlock, spin_flag);

    return NULL;
}

NET_DEV *netdev_get_by_ipaddr(struct in_addr *ip_addr)
{
    irq_flags_t spin_flag;
    struct list_node *node = NULL;
    NET_DEV *netdev = NULL;

    spin_lock_irqsave(&netdev_spinlock, spin_flag);

    if (NULL == NETDEV_LIST)
    {
        spin_unlock_irqrestore(&netdev_spinlock, spin_flag);
        return NULL;
    }

    for (node = &(NETDEV_LIST->node); node; node = list_next(node, &(NETDEV_LIST->node)))
    {
        netdev = list_entry(node, NET_DEV, node);
        if (netdev && ip_addr_cmp(&(netdev->ll_info.ethernet.ip_addr), ip_addr))
        {
            spin_unlock_irqrestore(&netdev_spinlock, spin_flag);
            return netdev;
        }
    }

    spin_unlock_irqrestore(&netdev_spinlock, spin_flag);

    return NULL;
}

NET_DEV *netdev_get_by_name(const char *name)
{
    irq_flags_t spin_flag;
    struct list_node *node = NULL;
    NET_DEV *netdev = NULL;
    size_t namelen;

    spin_lock_irqsave(&netdev_spinlock, spin_flag);
    if (NULL == name || NULL == NETDEV_LIST)
    {
        spin_unlock_irqrestore(&netdev_spinlock, spin_flag);
        return NULL;
    }

    namelen = strnlen(name, NETDEV_NAME_MAX) + 1;

    for (node = &(NETDEV_LIST->node); node; node = list_next(node, &(NETDEV_LIST->node)))
    {
        netdev = list_entry(node, NET_DEV, node);
        if (netdev && (strncmp(netdev->name, name, namelen) == 0))
        {
            spin_unlock_irqrestore(&netdev_spinlock, spin_flag);
            return netdev;
        }
    }

    spin_unlock_irqrestore(&netdev_spinlock, spin_flag);

    return NULL;
}

NET_DEV *netdev_get_by_unit(const int unit)
{
    irq_flags_t spin_flag;
    struct list_node *node = NULL;
    NET_DEV *netdev = NULL;

    spin_lock_irqsave(&netdev_spinlock, spin_flag);

    if (NULL == NETDEV_LIST)
    {
        spin_unlock_irqrestore(&netdev_spinlock, spin_flag);
        return NULL;
    }

    for (node = &(NETDEV_LIST->node); node; node = list_next(node, &(NETDEV_LIST->node)))
    {
        netdev = list_entry(node, NET_DEV, node);
        if (netdev && (netdev->netdev_unit == unit))
        {
            spin_unlock_irqrestore(&netdev_spinlock, spin_flag);
            return netdev;
        }
    }

    spin_unlock_irqrestore(&netdev_spinlock, spin_flag);

    return NULL;
}

NET_DEV *netdev_get_default(void)
{
    return DEFAULT_NETDEV;
}

short int netdev_flags_to_posix_flags(NET_DEV *ndev)
{
    short int flags = 0;

    if (netdev_is_up(ndev))
    {
        flags |= IFF_UP;
    }

    if (netdev_is_link_up(ndev))
    {
        flags |= IFF_RUNNING;
    }

    if (!(ndev->flags & NETDEV_FLAG_ETHARP))
    {
        flags |= IFF_NOARP;
    }

    if (ndev->flags & NETDEV_FLAG_BROADCAST)
    {
        flags |= IFF_BROADCAST;
    }

    if (ndev->flags & NETDEV_FLAG_IGMP)
    {
        flags |= IFF_MULTICAST;
    }

    if (ndev->flags & NETDEV_FLAG_LOOKBACK)
    {
        flags |= IFF_LOOPBACK;
    }

    return flags;
}

void netdev_set_default(NET_DEV *netdev)
{
    if (netdev && (netdev != DEFAULT_NETDEV))
    {
        DEFAULT_NETDEV = netdev;

        if (netdev->ops && netdev->ops->set_default)
        {
            netdev->ops->set_default(netdev);
        }

        if (g_netdev_default_change_callback)
        {
            g_netdev_default_change_callback(netdev, NETDEV_CB_DEFAULT_CHANGE);
        }
        KLOG_D("Setting default network interface device name(%s) successfully.", netdev->name);
    }
}

void netdev_set_default_change_callback(netdev_callback_fn register_callback)
{
    g_netdev_default_change_callback = register_callback;
}

int netdev_set_up(NET_DEV *netdev)
{
    int err = 0;

    ASSERT(netdev);

    if (!netdev->ops || !netdev->ops->set_up)
    {
        KLOG_E("The network interface device(%s) not support to set status.", netdev->name);
        return -ERROR;
    }

    if (netdev_is_up(netdev))
    {
        return OK;
    }

    /* 协议栈应负责与NET_DEV状态位的同步，如LwIP会主动调用lwip_netdev_sync_flags() */
    err = netdev->ops->set_up(netdev);

    return err;
}

int netdev_set_down(NET_DEV *netdev)
{
    int err;

    ASSERT(netdev);

    if (!netdev->ops || !netdev->ops->set_down)
    {
        KLOG_E("The network interface device(%s) not support to set status.", netdev->name);
        return -ERROR;
    }

    if (!netdev_is_up(netdev))
    {
        return OK;
    }

    /* 协议栈应负责与NET_DEV状态位的同步，如LwIP会主动调用lwip_netdev_sync_flags() */
    err = netdev->ops->set_down(netdev);

    if (OK == err)
    {
        (void)clear_static_routes_for_dev(netdev->name);
    }

    return err;
}

int netdev_set_link_up(NET_DEV *netdev)
{
    int err = 0;

    ASSERT(netdev);

    if (!netdev->ops || !netdev->ops->set_link_up)
    {
        KLOG_E("The network interface device(%s) not support to set link status.", netdev->name);
        return -ERROR;
    }

    if (netdev_is_link_up(netdev))
    {
        return OK;
    }

    err = netdev->ops->set_link_up(netdev);

#ifdef CONFIG_ARP_PHY_CHANGE_TO_LINKUP
    if (err == OK)
    {
        eth_arp_announce(NETDEV_TO_ETHDEV(netdev));
    }
#endif

    return err;
}

int netdev_set_link_down(NET_DEV *netdev)
{
    int err;

    ASSERT(netdev);

    if (!netdev->ops || !netdev->ops->set_link_down)
    {
        KLOG_E("The network interface device(%s) not support to set status.", netdev->name);
        return -ERROR;
    }

    if (!netdev_is_link_up(netdev))
    {
        return OK;
    }

    err = netdev->ops->set_link_down(netdev);

    return err;
}

int netdev_set_dhcp(NET_DEV *netdev, int is_enable)
{
    int err;
    ASSERT(netdev);

    if (!netdev->ops || !netdev->ops->set_dhcp)
    {
        KLOG_E("The network interface device(%s) not support to set DHCP status.", netdev->name);
        return -ERROR;
    }

    if (is_enable == 0 && netdev_is_dhcp_enabled(netdev) == is_enable)
    {
        return OK;
    }

    err = netdev->ops->set_dhcp(netdev, is_enable);

    netdev_sync_dhcp_status(netdev, is_enable);

    return err;
}

int netdev_set_ipaddr(NET_DEV *netdev, const struct in_addr *ip_addr)
{
    int err;
    ASSERT(netdev);
    ASSERT(ip_addr);

    if (!netdev->ops || !netdev->ops->set_addr_info)
    {
        KLOG_E("The network interface device(%s) not support to set IP address.", netdev->name);
        return -ERROR;
    }

#ifdef CONFIG_TCPIP
    if (netdev_is_dhcp_enabled(netdev))
    {
        if (netdev_set_dhcp(netdev, false) < 0)
        {
            return -ERROR;
        }
    }
#endif

    err = netdev->ops->set_addr_info(netdev, (struct in_addr *)ip_addr, NULL, NULL);

    netdev_sync_ipaddr(netdev, ip_addr);

    return err;
}

int netdev_set_mac(NET_DEV *netdev, unsigned long arg)
{
    int err;
    ASSERT(netdev);

    if (!netdev->ops || !netdev->ops->set_mac)
    {
        KLOG_E("The network interface device(%s) not support to set IP address.", netdev->name);
        return -ENOTSUP;
    }

    if (!arg)
    {
        KLOG_E("The MAC arg can not be NULL.");
        return -EINVAL;
    }

    err = netdev->ops->set_mac(netdev, arg);

    return err;
}

int netdev_set_netmask(NET_DEV *netdev, const struct in_addr *netmask)
{
    int err;
    ASSERT(netdev);
    ASSERT(netmask);

    if (!netdev->ops || !netdev->ops->set_addr_info)
    {
        KLOG_E("The network interface device(%s) not support to set netmask address.",
               netdev->name);
        return -ERROR;
    }

#ifdef CONFIG_TCPIP
    if (netdev_is_dhcp_enabled(netdev))
    {
        if (netdev_set_dhcp(netdev, false) < 0)
        {
            return -ERROR;
        }
    }
#endif

    err = netdev->ops->set_addr_info(netdev, NULL, (struct in_addr *)netmask, NULL);

    netdev_sync_netmask(netdev, netmask);

    return err;
}

int netdev_set_gw(NET_DEV *netdev, const struct in_addr *gw)
{
    int err;
    ASSERT(netdev);
    ASSERT(gw);

    if (!netdev->ops || !netdev->ops->set_addr_info)
    {
        KLOG_E("The network interface device(%s) not support to set gateway address.",
               netdev->name);
        return -ERROR;
    }

#ifdef CONFIG_TCPIP
    if (netdev_is_dhcp_enabled(netdev))
    {
        if (netdev_set_dhcp(netdev, false) < 0)
        {
            return -ERROR;
        }
    }
#endif

    err = netdev->ops->set_addr_info(netdev, NULL, NULL, (struct in_addr *)gw);

    netdev_sync_gw(netdev, gw);

    return err;
}

void netdev_set_status_callback(NET_DEV *netdev, netdev_callback_fn status_callback)
{
    ASSERT(netdev);
    ASSERT(status_callback);

    netdev->status_callback = status_callback;
}

void netdev_set_addr_callback(NET_DEV *netdev, netdev_callback_fn addr_callback)
{
    ASSERT(netdev);
    ASSERT(addr_callback);

    netdev->addr_callback = addr_callback;
}

void netdev_sync_ipaddr(NET_DEV *netdev, const struct in_addr *ip_addr)
{
    ASSERT(ip_addr);

    if (netdev && ip_addr_cmp(&(netdev->ll_info.ethernet.ip_addr), ip_addr) == 0)
    {
        ip_addr_copy(&netdev->ll_info.ethernet.ip_addr, ip_addr);

        if (netdev->addr_callback)
        {
            netdev->addr_callback(netdev, NETDEV_CB_ADDR_IP);
        }
    }
}

void netdev_sync_netmask(NET_DEV *netdev, const struct in_addr *netmask)
{
    ASSERT(netmask);

    if (netdev && ip_addr_cmp(&(netdev->ll_info.ethernet.netmask), netmask) == 0)
    {
        ip_addr_copy(&netdev->ll_info.ethernet.netmask, netmask);

        if (netdev->addr_callback)
        {
            netdev->addr_callback(netdev, NETDEV_CB_ADDR_NETMASK);
        }
    }
}

void netdev_sync_gw(NET_DEV *netdev, const struct in_addr *gw)
{
    ASSERT(gw);

    if (netdev && ip_addr_cmp(&(netdev->ll_info.ethernet.gw), gw) == 0)
    {
        ip_addr_copy(&netdev->ll_info.ethernet.gw, gw);

        if (netdev->addr_callback)
        {
            netdev->addr_callback(netdev, NETDEV_CB_ADDR_GATEWAY);
        }
    }
}

void netdev_sync_dhcp_status(NET_DEV *netdev, int is_enable)
{
    if (netdev && netdev_is_dhcp_enabled(netdev) != is_enable)
    {
        if (is_enable)
        {
            netdev->flags |= NETDEV_FLAG_DHCP;
        }
        else
        {
            netdev->flags &= ~NETDEV_FLAG_DHCP;
        }

        if (netdev->status_callback)
        {
            netdev->status_callback(netdev, is_enable ? NETDEV_CB_STATUS_DHCP_ENABLE
                                                      : NETDEV_CB_STATUS_DHCP_DISABLE);
        }
    }
}

NET_DEV *netdev_install_loopback(void *lookback_dev, netdev_install_cb callback,
                                 void *netstack_data)
{
    ETH_DEV *ethdev = NULL;
    struct list_node *node = NULL;
    NET_DEV *cur_netdev = NULL;
    irq_flags_t spin_flag;

    if (lookback_dev == NULL)
    {
        return NULL;
    }

    ethdev = (ETH_DEV *)lookback_dev;

    netdev_eth_lo = (NET_DEV *)calloc(1, sizeof(NET_DEV));
    if (netdev_eth_lo == NULL)
    {
        return NULL;
    }

    memset(netdev_eth_lo, 0, sizeof(NET_DEV));

    inet_pton(AF_INET, "127.0.0.1", &(netdev_eth_lo->ll_info.ethernet.ip_addr));
    inet_pton(AF_INET, "127.0.0.1", &(netdev_eth_lo->ll_info.ethernet.gw));
    inet_pton(AF_INET, "255.0.0.0", &(netdev_eth_lo->ll_info.ethernet.netmask));

    netdev_eth_lo->status_callback = NULL;
    netdev_eth_lo->addr_callback = NULL;

    netdev_eth_lo->ll_hdrlen = ETH_HDRLEN;

    netdev_eth_lo->ll_info.ethernet.pkt_stats.in_bytes = 0;
    netdev_eth_lo->ll_info.ethernet.pkt_stats.out_bytes = 0;
    netdev_eth_lo->ll_info.ethernet.pkt_stats.in_pacs = 0;
    netdev_eth_lo->ll_info.ethernet.pkt_stats.out_pacs = 0;
    netdev_eth_lo->ll_info.ethernet.pkt_stats.din_pacs = 0;
    netdev_eth_lo->ll_info.ethernet.pkt_stats.dout_pacs = 0;
    netdev_eth_lo->ll_info.ethernet.pkt_stats.ein_pacs = 0;
    netdev_eth_lo->ll_info.ethernet.pkt_stats.eout_pacs = 0;

    sprintf(netdev_eth_lo->name, "lo");

    netdev_eth_lo->link_data = (void *)ethdev;
    ethdev->netdev = netdev_eth_lo;

    netdev_eth_lo->ops = eth_stack_netdev_ops;
    netdev_eth_lo->netdev_unit = netdev_count++;

    netdev_eth_lo->node = LIST_HEAD_INIT(netdev_eth_lo->node);

    spin_lock_irqsave(&netdev_spinlock, spin_flag);

    if (NULL == NETDEV_LIST)
    {
        NETDEV_LIST = netdev_eth_lo;
    }
    else
    {
        netdev_eth_lo->node.next = &NETDEV_LIST->node;

        /* 查找到NETDEV_LIST链上最后一个设备 */
        for (node = &(NETDEV_LIST->node); node; node = list_next(node, &(NETDEV_LIST->node)))
        {
            cur_netdev = list_entry(node, NET_DEV, node);
            if (cur_netdev->node.next == &NETDEV_LIST->node)
            {
                break;
            }
        }

        cur_netdev->node.next = &netdev_eth_lo->node;
        netdev_eth_lo->node.prev = &cur_netdev->node;
        NETDEV_LIST->node.prev = &netdev_eth_lo->node;

        /* 将环回网卡的netdev_unit设置为1 */
        netdev_eth_lo->netdev_unit = netdev_eth_lo->netdev_unit ^ NETDEV_LIST->netdev_unit;
        NETDEV_LIST->netdev_unit = netdev_eth_lo->netdev_unit ^ NETDEV_LIST->netdev_unit;
        netdev_eth_lo->netdev_unit = netdev_eth_lo->netdev_unit ^ NETDEV_LIST->netdev_unit;

        NETDEV_LIST = netdev_eth_lo;
    }

    spin_unlock_irqrestore(&netdev_spinlock, spin_flag);

    /* 必须主动进行一次状态同步 */
    callback(netdev_eth_lo, netstack_data);
    netdev_eth_lo->flags |= NETDEV_FLAG_LOOKBACK;

    return netdev_eth_lo;
}
