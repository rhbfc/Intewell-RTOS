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
#include <list.h>
#include <stdio.h>
#include <stdlib.h>
#include <spinlock.h>
#include <netinet/in.h>
#include <net/route.h>
#include <net/netdev.h>
#include <net/ethernet_dev.h>
#include <net/netdev_route.h>

#ifdef CONFIG_LWIP_2_2_0
#include <lwip/netif.h>
#endif

DEFINE_SPINLOCK (STATIC_ROUTE_LOCK);

LIST_HEAD(STATIC_ROUTE_LIST);

static inline bool check_route_list_entry(static_route_item_t *rti, struct sockaddr_in *dst, struct sockaddr_in *gw, struct sockaddr_in *mask)
{
    return (rti->dest.s_addr == dst->sin_addr.s_addr
            && rti->gateway.s_addr == gw->sin_addr.s_addr
            && rti->netmask.s_addr == mask->sin_addr.s_addr);
}

/* TODO 目前add支持修改网关，后续增加路由功能 */
int route_list_add (NET_DEV *ndev, struct sockaddr_in *dst, struct sockaddr_in *gw, struct sockaddr_in *mask,
    uint32_t rt_flags, bool is_default)
{
    static_route_item_t *rti = NULL;
    static_route_item_t *__rti = NULL;
    long spin_flags;

    spin_lock_irqsave (&STATIC_ROUTE_LOCK, spin_flags);

    list_for_each_entry (__rti, &STATIC_ROUTE_LIST, node)
    {
        if (__rti->ndev == ndev && check_route_list_entry(__rti, dst, gw, mask))
        {
            rti = __rti;
            break;
        }
    }

    if (NULL != rti)
    {
        spin_unlock_irqrestore (&STATIC_ROUTE_LOCK, spin_flags);
        return -EEXIST;
    }

    rti = calloc (1, sizeof (static_route_item_t));
    if (NULL == rti)
    {
        spin_unlock_irqrestore (&STATIC_ROUTE_LOCK, spin_flags);
        return -ENOMEM;
    }

    rti->node = LIST_HEAD_INIT (rti->node);
    rti->ndev = ndev;
    rti->dest = dst->sin_addr;
    rti->gateway = gw->sin_addr;
    rti->netmask = mask->sin_addr;
    rti->is_default = is_default;
    rti->flags = rt_flags;

    list_add_tail (&rti->node, &STATIC_ROUTE_LIST);

    spin_unlock_irqrestore (&STATIC_ROUTE_LOCK, spin_flags);

    if (is_default && (netdev_set_gw(ndev, &gw->sin_addr) != OK))
    {
        (void)route_list_del(ndev, dst, gw, mask);
        return -EINVAL;
    }

    return OK;
}

int route_list_del (NET_DEV *ndev, struct sockaddr_in *dst, struct sockaddr_in *gw, struct sockaddr_in *mask)
{
    static_route_item_t *rti = NULL;
    static_route_item_t *__rti = NULL;
    bool f_change_gw = false;
    struct in_addr new_gw = { .s_addr = 0 };
    long spin_flags;

    spin_lock_irqsave (&STATIC_ROUTE_LOCK, spin_flags);

    list_for_each_entry (__rti, &STATIC_ROUTE_LIST, node)
    {
        if (check_route_list_entry(__rti, dst, gw, mask))
        {
            if (ndev && ndev != __rti->ndev)
            {
                continue;
            }
            rti = __rti;
            ndev = rti->ndev;
            break;
        }
    }

    if (NULL == rti)
    {
        spin_unlock_irqrestore (&STATIC_ROUTE_LOCK, spin_flags);
        return -ESRCH;
    }

    if (ndev && rti->gateway.s_addr != 0 && rti->gateway.s_addr == ndev->ll_info.ethernet.gw.s_addr)
    {
        f_change_gw = true;
    }

    list_del (&rti->node);
    free (rti);

    /* Find next available gateway */
    if (f_change_gw && ndev)
    {
        list_for_each_entry (__rti, &STATIC_ROUTE_LIST, node)
        {
            if (check_route_list_entry(__rti, dst, gw, mask))
            {
                if (ndev == __rti->ndev && __rti->gateway.s_addr != 0)
                {
                    new_gw.s_addr = __rti->gateway.s_addr;
                    break;
                }
            }
        }
    }

    spin_unlock_irqrestore (&STATIC_ROUTE_LOCK, spin_flags);

    if (f_change_gw)
    {
        (void) netdev_set_gw(ndev, &new_gw);
    }

    return OK;
}

NET_DEV *route_list_search (const struct in_addr const* dst, struct in_addr *netmask)
{
    static_route_item_t *rt = NULL;
    static_route_item_t *rt_default = NULL;
    static_route_item_t *rti = NULL;
    NET_DEV *target = NULL;
    irq_flags_t spin_flags;

    spin_lock_irqsave (&STATIC_ROUTE_LOCK, spin_flags);

    list_for_each_entry (rti, &STATIC_ROUTE_LIST, node)
    {
        if ((0 == rti->dest.s_addr) && (NULL == rt_default))
        {
            rt_default = rti;
        }
        else if ((rti->dest.s_addr == dst->s_addr) ||
            ((rti->dest.s_addr & rti->netmask.s_addr) == (dst->s_addr & rti->netmask.s_addr)))
        {
            rt = rti;
            break;
        }
    }

    if (NULL == rt)
    {
        rt = rt_default;
    }

    if (rt != NULL)
    {
        target = rt->ndev;
        netmask->s_addr = rt->netmask.s_addr;
    }

    spin_unlock_irqrestore (&STATIC_ROUTE_LOCK, spin_flags);

    return target;
}

int clear_static_routes_for_dev(char *name)
{
    NET_DEV *ndev;
    static_route_item_t *rti = NULL, *__rti = NULL;
    struct in_addr gwaddr = { .s_addr = 0 };
    long spin_flags;

    ndev = netdev_get_by_name(name);
    if (NULL == ndev)
    {
        return -ENODEV;
    }

    spin_lock_irqsave (&STATIC_ROUTE_LOCK, spin_flags);

    list_for_each_entry_safe (rti, __rti, &STATIC_ROUTE_LIST, node)
    {
        if (rti->ndev == ndev)
        {
            list_delete(&rti->node);
            free(rti);
        }
    }

    spin_unlock_irqrestore (&STATIC_ROUTE_LOCK, spin_flags);

    netdev_set_gw(ndev, &gwaddr);

    return OK;
}
