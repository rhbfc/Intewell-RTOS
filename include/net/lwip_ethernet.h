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

#ifndef __LWIP_ETHIF_H__
#define __LWIP_ETHIF_H__

ETH_NETPKT *eth_lwip_alloc_custom_pbuf (unsigned int len);
int eth_lwip_netpkt_to_desc(ETH_NETPKT *netpkt, void *desc_buf);
ETH_NETPKT *eth_lwip_alloc_pool_pbuf(unsigned int len);
int eth_lwip_netif_input (ETH_DEV *ethdev, struct pbuf *pb);
err_t eth_lwip_netif_linkoutput (struct netif *netif, struct pbuf *pb);
int eth_lwip_get_arp_entry (int i, char *ip, char *mac, unsigned char *state, char *dev_name);
void eth_lwip_arp_broadcast(ETH_DEV *ethdev);
int eth_lwip_netdev_install_callback (NET_DEV *ndev, struct netif *nif);
void *eth_lwip_device_init (ETH_DEV *ethdev, const char *name, const uint8_t *const mac_addr);
void *eth_lwip_loopback_init (ETH_DEV *ethdev_lo);
void eth_lwip_del_dev (struct netif *lwip_netif);
int eth_lwip_add_ip (ETH_DEV *ethdev, ETH_CFG_INFO *cfg_info);
int eth_lwip_del_ip_by_netif (ETH_DEV *ethdev, struct netif* nif);
void eth_lwip_data_to_stack (ETH_DEV *ethdev, ETH_NETPKT *netpkt);
struct netif *eth_lwip_bridge_create ();

static inline void eth_lwip_to_internal_name(ETH_DEV *eth, char *name)
{
    name[0] = eth->lwip_netif[0]->name[0];
    name[1] = eth->lwip_netif[0]->name[1];
    name[2] = eth->lwip_netif[0]->num % 10 + '0';
    name[3] = '\0';
}

#endif
