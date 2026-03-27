/****************************************************************************
 * drivers/virtio/virtio-net.h
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/
 
/****************************************************************************
 * 本文件基于 Apache NuttX 对应 virtio 实现移植到当前 RTOS，
 * 并已针对本地接口和平台环境进行了修改。
 ****************************************************************************/

#ifndef __DRIVERS_VIRTIO_VIRTIO_NET_H
#define __DRIVERS_VIRTIO_VIRTIO_NET_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Public Data
 ****************************************************************************/

#ifdef __cplusplus
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

#include <ttos.h>
#include <net/ethernet_dev.h>
#include "virtio.h"

#define VIRTIO_NET_NAME "virtionet"
#define VIRTIO_NET_F_MAC 5

/* Virtio net header size and packet buffer size */
#define VIRTIO_NET_HDRSIZE   (sizeof (struct virtio_net_hdr))
#define VIRTIO_NET_LLHDRSIZE (sizeof (struct virtio_net_llhdr))
#define VIRTIO_NET_BUFSIZE   (CONFIG_NET_ETH_PKTSIZE + CONFIG_NET_GUARDSIZE)

/* Virtio net virtqueue index and number */
#define VIRTIO_NET_RX  0
#define VIRTIO_NET_TX  1
#define VIRTIO_NET_NUM 2

/* Virtio net header, just define it here for further, now only use it
 * to calculate the virto net header size, see marco VIRTIO_NET_HDRSIZE
 */

struct virtio_net_hdr
{
    uint8_t  flags;
    uint8_t  gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
} __packed;


/* Virtio Link Layer Header, follow shows the iob buffer layout:
 *
 * |<-- CONFIG_NET_LL_GUARDSIZE -->|
 * +---------------+---------------+------------+------+     +-------------+
 * | Virtio Header |  ETH Header   |    data    | free | --> | next netpkt |
 * +---------------+---------------+------------+------+     +-------------+
 * |               |<--------- datalen -------->|
 * ^base           ^data
 *
 * CONFIG_NET_LL_GUARDSIZE >= VIRTIO_NET_LLHDRSIZE + ETH_HDR_SIZE
 *                          = sizeof(uintptr) + 10 + 14
 *                          = 32 (64-Bit)
 *                          = 28 (32-Bit)
 */

struct virtio_net_llhdr
{
    ETH_NETPKT *netpkt;  /* Netpaket pointer */
    struct virtio_net_hdr vhdr; /* Virtio net header */
} __packed;


/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/
struct virtio_net_priv_s
{
    ETH_DEV ethdev;
    struct virtio_device *vdev;   /* Virtio device pointer */
    int bufnum; /* TX and RX Buffer number */
    atomic_t quota[2];
    ttos_spinlock_t spin_lock[VIRTIO_NET_NUM];
};

enum netbuf_type
{
    NETPKT_RX,
    NETPKT_TX,
    NETPKT_TYPENUM
};

/* 为struct virtio_net_llhdr预留的空间 */
#define CONFIG_NET_HDR_GUARDSIZE (sizeof(struct virtio_net_llhdr) + ETH_HDRLEN)
#define NETPKT_DATA_SIZE (ETHERNET_DEFAULT_MTU + ETH_HDRLEN)
#define NETPKT_TOTAL_SIZE (ETHERNET_DEFAULT_MTU + CONFIG_NET_HDR_GUARDSIZE)
#define NETPKT_DATAPTR(pkt)  (&(pkt->buf[CONFIG_NET_HDR_GUARDSIZE]))

static_assert (CONFIG_NET_HDR_GUARDSIZE >= VIRTIO_NET_LLHDRSIZE + ETH_HDRLEN, "CONFIG_NET_HDR_GUARDSIZE cannot be less than ETH_HDRLEN"" + VIRTIO_NET_LLHDRSIZE");

int virtio_register_net_driver (void);

#undef EXTERN
#ifdef __cplusplus
}
#endif

#endif /* __DRIVERS_VIRTIO_VIRTIO_NET_H */
