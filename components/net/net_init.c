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

#include <net/ethernet_dev.h>
#include <net/netdev.h>
#include <ttos_init.h>

#define KLOG_TAG "NETWORK_INIT"
#include <klog.h>

extern int ttos_lwip_init();
extern void phy_subsystem_init();
extern void eth_pcap_register_sockif();

static int ttos_net_init()
{
#if IS_ENABLED(CONFIG_LWIP_2_2_0)
    ttos_lwip_init();
#endif

#ifdef CONFIG_SUPPORT_PCAP_TOOL
    eth_pcap_register_sockif();
#endif

    return 0;
}

INIT_EXPORT_COMPONENTS(ttos_net_init, "Init Network and Protocol Stack");
