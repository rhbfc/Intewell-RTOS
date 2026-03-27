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

#ifndef __NET_PACKET__
#define __NET_PACKET__

struct socket_stat {
    uint8_t  flags;
    size_t   waiting_inbytes;
    struct aio_info aio;
    struct kpollfd *kfd;
    int rx_event;
    int tx_event;
    int err_event;
    ttos_spinlock_t rw_lock;
    MUTEX_ID mutex_lock;
};

typedef struct packet_sock
{
    struct list_node node;
    struct socket_stat stat;
    uint16_t proto;

    ETH_DEV   *eth;
    SEMA_ID    pkt_sem;
    T_UWORD    rcv_ticks;

    struct {
        ETH_NETPKT *head;
        ETH_NETPKT *tail;
    } pkt_list;
    uint32_t pkt_count;
} packet_sock_t;

struct active_packet_sockets {
    uint32_t count;
    uint32_t max_count;

    /* 存放可用已激活sockets指针的堆上数组 */
    packet_sock_t **socks;
};

extern ttos_spinlock_t ACTIVE_PACKET_SOCKETS_LOCK;
extern struct active_packet_sockets ACTIVE_PACKET_SOCKETS;

int packet_input(ETH_DEV *eth, const ETH_NETPKT *pkt_origin);
void packet_input_filter(ETH_DEV *ethdev, ETH_NETPKT *netpkt);

#endif
