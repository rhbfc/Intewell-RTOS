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
#include <system/kconfig.h>
#include <net/ethernet_dev.h>
#include <net/if_arp.h>
#include <netinet/if_ether.h>
#include <net/netdev.h>
#include <net/packet.h>

#include <symtab.h>

#if IS_ENABLED(CONFIG_LWIP_2_2_0)
#include <lwip/netif.h>
#include <lwip/pbuf.h>
#include <net/lwip_ethernet.h>
#endif

extern int pcap_enable_flag;

KSYM_EXPORT(ETH_DATA_TO_STACK);

/* 数据传递至协议栈 */
void ETH_DATA_TO_STACK(ETH_DEV *ethdev, ETH_NETPKT *netpkt)
{
#ifdef CONFIG_SUPPORT_PCAP_TOOL
    struct pcap_nsock *pnsock;

    if (pcap_enable_flag)
    {
        if (TTOS_OK == TTOS_ObtainMutex(PCAP_SOCKLIST_MUTEX, TTOS_MUTEX_WAIT_FOREVER))
        {
            list_for_each_entry(pnsock, &PCAP_NSOCK_LIST, node)
            {
                if (ethdev == pnsock->eth)
                {
                    TTOS_SendMsgq(pnsock->msgq, netpkt->buf, netpkt->len, 0, 0);
                }
            }
            TTOS_ReleaseMutex(PCAP_SOCKLIST_MUTEX);
        }
    }
#endif

#if defined(CONFIG_TCPIP)
#if defined(CONFIG_SUPPORT_AF_PACKET)
    packet_input_filter(ethdev, netpkt);
#endif /* CONFIG_SUPPORT_AF_PACKET */

#if IS_ENABLED(CONFIG_LWIP_2_2_0)
    eth_lwip_data_to_stack(ethdev, netpkt);
#endif /* IS_ENABLED(CONFIG_LWIP_2_2_0) */
#else
    packet_input(ethdev, netpkt);
#endif /* IS_ENABLED(CONFIG_TCPIP) */
}
