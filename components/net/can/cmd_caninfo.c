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
#include <net/can_dev.h>
#include <shell.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ttos.h>
#include <unistd.h>

#ifdef CONFIG_SHELL
extern struct list_head CAN_DEV_LIST;
extern ttos_spinlock_t CAN_DEV_LOCK;

static void afcan_info_output(CAN_DEV *candev)
{
    AFCAN_STATS *afcan_stats = NULL;
    afcan_stats = &(candev->netdev->ll_info.can.can_stats);

    printk("%s    Unit:%d Device:[%s]\n", candev->netdev->name, candev->netdev->netdev_unit, candev->dev_info->dev_name);

    printk("        Link Encap:CAN ");

    printk("Status:");
    if (netdev_is_up(candev->netdev))
    {
        printk("UP\n");
    }

    printk("        RX Frames:%llu Bytes:%llu Drop Frames:%llu Bytes:%llu\n",
           afcan_stats->in_frames, afcan_stats->in_bytes, afcan_stats->din_frames, afcan_stats->din_bytes);
    printk("        TX Frames:%llu Bytes:%llu Error Frames:%llu Bytes:%llu\n\n",
           afcan_stats->out_frames, afcan_stats->out_bytes, afcan_stats->eout_frames, afcan_stats->eout_bytes);
}

static int can_info(int argc, char *argv[])
{
    char *can_name = argv[1];
    CAN_DEV *candev = NULL;
    NET_DEV *netdev = NULL;
    long eth_flags;
    AFCAN_STATS *can_stats = NULL;

    if (argc < 2)
    {
        spin_lock_irqsave(&CAN_DEV_LOCK, eth_flags);

        list_for_each_entry(candev, &CAN_DEV_LIST, node)
        {
            afcan_info_output(candev);
        }

        spin_unlock_irqrestore(&CAN_DEV_LOCK, eth_flags);
    }
    else
    {
        netdev = netdev_get_by_name(can_name);
        if (candev == NULL)
        {
            printk("The specified Can Device was not Found!\n");
            return -1;
        }

        afcan_info_output(candev);
    }
    
    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) |
                     SHELL_CMD_DISABLE_RETURN, caninfo, can_info, CAN Device Infomation);
#endif