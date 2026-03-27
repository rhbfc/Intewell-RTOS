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

#include <inttypes.h>
#include <net/ethernet_dev.h>
#include <net/netdev.h>
#include <net/phy_dev.h>
#include <shell.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ttos.h>
#include <unistd.h>

#ifdef CONFIG_SHELL
extern unsigned int nebuf_alloc_err;
extern unsigned int pbuf_alloc_err;
extern unsigned int netconn_alloc_err;
extern unsigned int other_pool_err;

static void af_packet_info_output(ETH_DEV *ethdev)
{
    AFPACKET_STATS *pkt_stats = NULL;
    pkt_stats = &(ethdev->netdev->ll_info.ethernet.afpkt_stats);

    printk("%s    Unit:%d Driver:%s PHY:%s\n", ethdev->netdev->name, ethdev->netdev->netdev_unit, ethdev->drv_info->drv_name,
                                               ethdev->phydev != NULL ? ethdev->phydev->phy_drv->name : "");

    printk("        Link Encap:Ethernet HWadd:[%02x:%02x:%02x:%02x:%02x:%02x]\n",
           ethdev->netdev->hwaddr[0], ethdev->netdev->hwaddr[1], ethdev->netdev->hwaddr[2],
           ethdev->netdev->hwaddr[3], ethdev->netdev->hwaddr[4], ethdev->netdev->hwaddr[5]);

    printk("        Status:");
    if (netdev_is_up(ethdev->netdev))
    {
        printk("UP ");

        if (ethdev->phydev != NULL)
        {
            if (ethdev->phy_status & IFM_ACTIVE)
            {
                printk("RUNNING ");

                switch (IFM_SUBTYPE(ethdev->phy_media))
                {
                case IFM_1000_T:
                    printk("1000Mbit ");
                    break;

                case IFM_100_TX:
                    printk("100Mbit ");
                    break;

                case IFM_10_T:
                    printk("10Mbit ");
                    break;

                default:
                    break;
                }

                if (ethdev->phy_media & IFM_FDX)
                {
                    printk("Full Duplex\n");
                }
                else
                {
                    printk("Half Duplex\n");
                }
            }
            else
            {
                printk("\n");
            }
        }
        else
        {
            printk("\n");
        }
    }

    printk("        RX packets:%llu bytes:%llu Drop packets:%llu bytes:%llu\n", pkt_stats->in_pacs, pkt_stats->in_bytes, pkt_stats->din_pacs, pkt_stats->din_bytes);
    printk("        TX packets:%llu bytes:%llu Error packets:%llu bytes:%llu\n\n", pkt_stats->out_pacs, pkt_stats->out_bytes, pkt_stats->eout_pacs, pkt_stats->eout_bytes);
}

static int eth_info_output(ETH_DEV *ethdev, void *arg)
{
    (void)arg;

    if (ethdev == NULL)
    {
        return -1;
    }

    if (ethdev->eth_unit == -1)
    {
        return 0;
    }

    af_packet_info_output(ethdev);
}

#ifdef CONFIG_LWIP_2_2_0
static void eth_lwip_err_info()
{
    printk("LwIP MemPool ERROR Info:\n");
    printk("        NETBUF:%d PBUF:%d NETCONN:%d Other:%d\n", nebuf_alloc_err, pbuf_alloc_err,
           netconn_alloc_err, other_pool_err);
}
#endif

static int eth_info(int argc, char *argv[])
{
    char *eth_name = argv[1];
    ETH_DEV *ethdev = NULL;
    NET_DEV *netdev = NULL;
    AFPACKET_STATS *pkt_stats = NULL;

    if (argc < 2)
    {
        eth_device_iterate(eth_info_output, NULL);
    }
    else
    {
        ethdev = eth_find_by_name(eth_name);
        if (ethdev == NULL || IS_LOOPBACK_DEV(ethdev))
        {
            printk("The specified ethernet device was not found!\n");
            return -1;
        }

        af_packet_info_output(ethdev);
    }

    netdev = netdev_get_default();
    printk("Default Network Device:[%s] Unit:[%d]\n\n", netdev->name, netdev->netdev_unit);

#ifdef CONFIG_LWIP_2_2_0
    eth_lwip_err_info();
#endif

    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) |
                     SHELL_CMD_DISABLE_RETURN,
                 ethinfo, eth_info, Ethernet Infomation);
#endif