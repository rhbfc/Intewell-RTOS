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

#include <stdint.h>
#include <errno.h>
#include <list.h>
#include <assert.h>
#include <atomic.h>

#include <net/net.h>
#include <netinet/in.h>
#include <net/if.h>
#include <netinet/if_ether.h>
#include <netpacket/packet.h>

#include <net/netdev.h>
#include <net/ethernet_dev.h>
#include <net/packet.h>

#undef KLOG_TAG
#define KLOG_TAG "PACKET"
#include <klog.h>

#ifndef CONFIG_AF_PACKET_CACHEABLE_ENTRIES
#define CONFIG_AF_PACKET_CACHEABLE_ENTRIES 4096
#endif

static packet_sock_t *find_active_packet_socket(NET_DEV *ndev, uint16_t proto, uint32_t *idx)
{
    irq_flags_t pl_flags;
    packet_sock_t *target = NULL;
    uint32_t i;

    spin_lock_irqsave(&ACTIVE_PACKET_SOCKETS_LOCK, pl_flags);

    if (*idx < ACTIVE_PACKET_SOCKETS.count)
    {
        for (i = *idx; i < ACTIVE_PACKET_SOCKETS.count; i += 1)
        {
            if ((ACTIVE_PACKET_SOCKETS.socks[i]->eth == (ETH_DEV *)ndev->link_data)
                && (ACTIVE_PACKET_SOCKETS.socks[i]->proto == __htons(ETH_P_ALL) || ACTIVE_PACKET_SOCKETS.socks[i]->proto == __htons(proto)))
            {
                target = ACTIVE_PACKET_SOCKETS.socks[i];
                break;
            }
        }
    }

    spin_unlock_irqrestore(&ACTIVE_PACKET_SOCKETS_LOCK, pl_flags);

    if (NULL == target)
    {
        *idx = (uint32_t)(-1);
    }
    else
    {
        *idx = i + 1;
    }

    return target;
}

int packet_input(ETH_DEV *eth, const ETH_NETPKT *pkt_origin)
{
    packet_sock_t *priv = NULL;
    ETH_NETPKT *new_pkt = NULL;
    ETH_NETPKT *pkt = NULL;
    short event_set = 0;
    irq_flags_t rw_flags;
    uint16_t proto;
    uint32_t idx = 0;
    bool multi_delivery = false;
    int ret;

    proto = eth_network_protocol_parse((ether_header_t *)pkt_origin->buf);

    do
    {
        priv = find_active_packet_socket(eth->netdev, proto, &idx);

        if (priv && (priv->stat.rx_event < CONFIG_AF_PACKET_CACHEABLE_ENTRIES))
        {
            new_pkt = eth_netpkt_clone(pkt_origin);
            if (NULL == new_pkt)
            {
                KLOG_E("packet_input: eth_netpkt_clone failed");
                continue;
            }

            pkt = new_pkt;

            if (!multi_delivery)
            {
                atomic64_inc(&eth->netdev->ll_info.ethernet.afpkt_stats.in_pacs);
                atomic64_add(&eth->netdev->ll_info.ethernet.afpkt_stats.in_bytes, pkt_origin->len);
            }

            ret = TTOS_ObtainMutex(priv->stat.mutex_lock, TTOS_WAIT_FOREVER);
            if (ret != TTOS_OK)
            {
                return -ttos_ret_to_errno(ret);
            }

            if (priv->pkt_list.head && priv->pkt_list.tail)
            {
                priv->pkt_list.tail->next = pkt;
                pkt->next = NULL;
                priv->pkt_list.tail = pkt;
            }
            else if ((!priv->pkt_list.head) && (!priv->pkt_list.tail))
            {
                priv->pkt_list.head = pkt;
                priv->pkt_list.tail = pkt;
                pkt->next = NULL;
            }
            else
            {
                assert(0);
            }

            priv->stat.rx_event += 1;

            TTOS_ReleaseMutex(priv->stat.mutex_lock);

            multi_delivery = true;

            event_set |= POLLIN;
            TTOS_ReleaseSema(priv->pkt_sem);

            if (priv->stat.kfd != NULL)
            {
                kpoll_notify(&priv->stat.kfd, 1, event_set, &priv->stat.aio);
            }
        }
    } while (priv != NULL);

    if (!multi_delivery)
    {
        atomic64_inc(&eth->netdev->ll_info.ethernet.afpkt_stats.din_pacs);
        atomic64_add(&eth->netdev->ll_info.ethernet.afpkt_stats.din_bytes, pkt_origin->len);
    }

    return 0;
}

void packet_input_filter(ETH_DEV *ethdev, ETH_NETPKT *netpkt)
{
    ETH_NETPKT *cur_pkt = netpkt;
    ETH_NETPKT *next_pkt = NULL;
    uint16_t pptp_type;

    /* 循环处理netpkt */
    while (cur_pkt)
    {
        next_pkt = cur_pkt->next;

        pptp_type = eth_network_protocol_parse((ether_header_t *)cur_pkt->buf);
        switch (pptp_type)
        {
            case ETH_P_ARP:
#if defined(CONFIG_SUPPORT_AF_PACKET_RECV_ARP) || defined(CONFIG_SUPPORT_AF_PACKET_RECV_ALL)
                packet_input(ethdev, cur_pkt);
#endif
                break;
            case ETH_P_IP:
#if defined(CONFIG_SUPPORT_AF_PACKET_RECV_ALL)
                packet_input(ethdev, cur_pkt);
#endif
                break;
            default:
                packet_input(ethdev, cur_pkt);
                break;
        }

        cur_pkt = next_pkt;
    }
}