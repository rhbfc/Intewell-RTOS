/****************************************************************************
 * drivers/virtio/virtio-net.c
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

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include "virtio-net.h"
#include <ttosBase.h>
#include <barrier.h>
#include <errno.h>
#include <net/if.h>
#include <net/netdev.h>
#include <netinet/if_ether.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <ttos_init.h>

#define KLOG_LEVEL KLOG_INFO
#define KLOG_TAG "VIRTIO_NET"
#include <klog.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* The definition of the struct virtio_net_config refers to the link
 * https://docs.oasis-open.org/virtio/virtio/v1.2/cs01/
 * virtio-v1.2-cs01.html#x1-2230004.
 */

struct virtio_net_config_s
{
    uint8_t mac[ETH_HW_ADDR_LEN]; /* VIRTIO_NET_F_MAC */
    uint16_t status;              /* VIRTIO_NET_F_STATUS */
    uint16_t max_virtqueue_pairs; /* VIRTIO_NET_F_MQ */
    uint16_t mtu;                 /* VIRTIO_NET_F_MTU */
    uint32_t speed;               /* VIRTIO_NET_F_SPEED_DUPLEX */
    uint8_t duplex;
    uint8_t rss_max_key_size; /* VIRTIO_NET_F_RSS */
    uint16_t rss_max_indirection_table_length;
    uint32_t supported_hash_types;
} __packed;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int virtio_net_ifup(ETH_DEV *ethdev);
static int virtio_net_ifdown(ETH_DEV *ethdev);
static int virtio_net_send(ETH_DEV *ethdev, ETH_NETPKT *netpkt);
static int virtio_net_recv(ETH_DEV *ethdev);
static int virtio_net_set_macaddr(ETH_DEV *ethdev, const char *hwaddr);
static int virtio_net_get_macaddr(ETH_DEV *ethdev, char *hwaddr);
static int virtio_net_ioctl(ETH_DEV *ethdev, int cmd, unsigned long arg);
static int virtio_net_probe(struct virtio_device *vdev);
static void virtio_net_remove(struct virtio_device *vdev);
extern int quota_fetch_dec(struct virtio_net_priv_s *virtio_ctl, enum netbuf_type type);
extern int quota_fetch_inc(struct virtio_net_priv_s *virtio_ctl, enum netbuf_type type);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct virtio_driver g_virtio_net_driver = {
    .device = VIRTIO_ID_NETWORK, /* device id */
    .driver.name = VIRTIO_NET_NAME,
    .probe = virtio_net_probe,   /* probe */
    .remove = virtio_net_remove, /* remove */
};

static ETH_DEV_FUNCS virtio_net_funcs = {.ifup = virtio_net_ifup,
                                         .ifdown = virtio_net_ifdown,
                                         .unload = NULL,
                                         .ioctl = virtio_net_ioctl,
                                         .send = virtio_net_send,
                                         .receive = virtio_net_recv,
                                         .mCastAddrAdd = NULL,
                                         .mCastAddrDel = NULL,
                                         .mCastAddrGet = NULL,
                                         .pollSend = NULL,
                                         .pollRcv = NULL,
                                         .mac_get = virtio_net_get_macaddr,
                                         .mac_set = virtio_net_set_macaddr};

static ETH_DRV_PARAM virtio_net_drv_param[] = {
    /* {"mdio_type", ETH_PARAM_INT32, {(void *)clause22}}, */
    {NULL, ETH_PARAM_END_OF_LIST, {NULL}}};

static ETH_DRV_INFO virtio_net_drv_info = {{NULL, NULL},
                                           VIRTIO_NET_NAME,
                                           "virtio net mmio driver",
                                           mmio,
                                           virtio_net_drv_param,
                                           0,
                                           0,
                                           &virtio_net_funcs,
                                           NULL};

/****************************************************************************
 * Private Functions
 ****************************************************************************/
int quota_fetch_dec(struct virtio_net_priv_s *virtio_ctl, enum netbuf_type type)
{
    return atomic_dec_return(&virtio_ctl->quota[type]);
}

int quota_fetch_inc(struct virtio_net_priv_s *virtio_ctl, enum netbuf_type type)
{
    return atomic_inc_return(&virtio_ctl->quota[type]);
}

int eth_quota_load(struct virtio_net_priv_s *virtio_ctl, enum netbuf_type type)
{
    return atomic_read(&virtio_ctl->quota[type]);
}

/****************************************************************************
 * Name: virtio_net_rxfill
 *        Fill the virtqueue with pbuf
 ****************************************************************************/

static void virtio_net_rxfill(ETH_DEV *ethdev)
{
    struct virtio_net_priv_s *virtio_ctl = (struct virtio_net_priv_s *)ethdev->dev_ctrl;
    struct virtqueue *vq = virtio_ctl->vdev->vrings_info[VIRTIO_NET_RX].vq;
    struct virtio_net_llhdr *hdr;
    struct virtqueue_buf vb;
    ETH_NETPKT *netpkt = NULL;
    int iov_cnt = 1;
    int cnt;
    int i;

    cnt = eth_quota_load(virtio_ctl, NETPKT_RX);

    for (i = 0; i < cnt; i += 1)
    {

        quota_fetch_dec (virtio_ctl, NETPKT_RX);

        netpkt = eth_netpkt_alloc(NETPKT_TOTAL_SIZE);
        if (netpkt == NULL)
        {
            quota_fetch_inc (virtio_ctl, NETPKT_RX);
            break;
        }

        /*
            netpkt                                                                 hdr
            │                                                                       │
            │                             ┌─────────────────────────────────────────┐
            │                             │                   ┌────┐                │     
            ↓                             │                   │    ↓                ↓ 
            ┌────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┐
            │     │     │    │     |   │    │            │         │                │                                            │
            │*base│*next│refs│flags|len│*buf│reserved_len|*reserved│[reserved space]│[virtio_net_llhdr][ETH_HDRLEN][ETHERNET_MTU]│
            │     │     │    │     |   │    │            │         │                │   *netpkt   vhdr                           │
            └────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┘
            ↑   │                                                                          │        ↑                            │
            └───│──────────────────────────────────────────────────────────────────────────┘      vb.buf                         │
            ↑   │                                                                                   │                            │
            └───┘                                                                                   └────────────vb.len──────────┘
         stack struct
            NETPKT_TOTAL_SIZE = sizeof(struct virtio_net_llhdr) + ETH_HDRLEN + ETHERNET_MTU
        */

        hdr = (struct virtio_net_llhdr *)(netpkt->buf);

        memset(&hdr->vhdr, 0, sizeof(hdr->vhdr));
        hdr->netpkt = netpkt;

        vb.buf = &hdr->vhdr;
        vb.len = NETPKT_TOTAL_SIZE - sizeof(ETH_NETPKT *);

        virtqueue_add_buffer(vq, &vb, 0, iov_cnt, hdr);
    }

    if (i > 0)
    {
        virtqueue_kick_lock (vq, &virtio_ctl->spin_lock[VIRTIO_NET_RX]);
    }
}

/****************************************************************************
 * Name: virtio_net_txfree
 ****************************************************************************/

static void virtio_net_txfree(struct virtio_net_priv_s *virtio_ctl)
{
    struct virtqueue *vq = virtio_ctl->vdev->vrings_info[VIRTIO_NET_TX].vq;
    struct virtio_net_llhdr *hdr;

    while (1)
    {
        /* Get buffer from tx virtqueue */
        hdr = virtqueue_get_buffer_lock (vq, NULL, NULL, &virtio_ctl->spin_lock[VIRTIO_NET_TX]);
        if (hdr == NULL)
        {
            break;
        }

        quota_fetch_inc(virtio_ctl, NETPKT_TX);
        free(hdr);
    }
}

/****************************************************************************
 * Name: virtio_net_ifup
 ****************************************************************************/

static int virtio_net_ifup(ETH_DEV *ethdev)
{
    struct virtio_net_priv_s *virtio_ctl = (struct virtio_net_priv_s *)ethdev->dev_ctrl;
    int i;

    /* Prepare interrupt and packets for receiving */

    for (i = 0; i < VIRTIO_NET_NUM; i++)
    {
        virtqueue_enable_cb_lock (virtio_ctl->vdev->vrings_info[i].vq, &virtio_ctl->spin_lock[i]);
    }

    return OK;
}

/****************************************************************************
 * Name: virtio_net_ifdown
 ****************************************************************************/

static int virtio_net_ifdown(ETH_DEV *ethdev)
{
    struct virtio_net_priv_s *virtio_ctl = (struct virtio_net_priv_s *)ethdev->dev_ctrl;
    int i;

    /* Disable the Ethernet interrupt */

    for (i = 0; i < VIRTIO_NET_NUM; i++)
    {
        virtqueue_disable_cb_lock (virtio_ctl->vdev->vrings_info[i].vq, &virtio_ctl->spin_lock[i]);
    }

    return OK;
}

/****************************************************************************
 * Name: virtio_net_send
 ****************************************************************************/
static int virtio_net_send(ETH_DEV *ethdev, ETH_NETPKT *netpkt)
{
    struct virtio_net_priv_s *virtio_ctl = (struct virtio_net_priv_s *)ethdev->dev_ctrl;
    struct virtqueue *vq = virtio_ctl->vdev->vrings_info[VIRTIO_NET_TX].vq;
    struct virtio_net_llhdr *hdr;
    struct virtqueue_buf vb;
    unsigned char *buf = NULL;
    int iov_cnt = 1;

    if (quota_fetch_dec(virtio_ctl, NETPKT_TX) <= 0)
    {
        quota_fetch_inc(virtio_ctl, NETPKT_TX);
        return -ENOMEM;
    }

    buf = memalign(CONFIG_NETPKT_ALIGNMENT_LEN, NETPKT_TOTAL_SIZE + VIRTIO_NET_LLHDRSIZE);
    hdr = (struct virtio_net_llhdr *)buf;
    memset(&hdr->vhdr, 0, sizeof(hdr->vhdr));
    hdr->netpkt = netpkt;

    eth_netpkt_to_desc((unsigned char *)hdr + VIRTIO_NET_LLHDRSIZE, netpkt);

    vb.buf = &hdr->vhdr;
    vb.len = netpkt->len + VIRTIO_NET_HDRSIZE;

    /* Add buffer to vq and notify the other side */
    virtqueue_add_buffer_lock (vq, &vb, iov_cnt, 0, hdr, &virtio_ctl->spin_lock[VIRTIO_NET_TX]);
    virtqueue_kick_lock (vq, &virtio_ctl->spin_lock[VIRTIO_NET_TX]);
    
    if (eth_quota_load (virtio_ctl, NETPKT_TX) <= (virtio_ctl->bufnum / 2))
    {
        virtio_net_txfree (virtio_ctl);
    }

    return OK;
}

/****************************************************************************
 * Name: virtio_net_recv
 ****************************************************************************/

static int virtio_net_recv(ETH_DEV *ethdev)
{
    struct virtio_net_priv_s *virtio_ctl = (struct virtio_net_priv_s *)ethdev->dev_ctrl;
    struct virtqueue *vq = virtio_ctl->vdev->vrings_info[VIRTIO_NET_RX].vq;
    struct virtio_net_llhdr *hdr;
    ETH_NETPKT *netpkt = NULL;
    ETH_NETPKT *pre_netpkt = NULL;
    ETH_NETPKT *first_netpkt = NULL;
    uint16_t pptp_type;
    uint32_t len;

    while (1)
    {
        /* Get received buffer form RX virtqueue */
        hdr = (struct virtio_net_llhdr *)virtqueue_get_buffer (vq, &len, NULL);
        if (hdr == NULL)
        {
            /* If we have no buffer left, enable RX callback. */
            virtqueue_enable_cb (vq);

            break;
        }

        netpkt = hdr->netpkt;
        netpkt->len = len - VIRTIO_NET_HDRSIZE;
        netpkt->buf = netpkt->buf + VIRTIO_NET_LLHDRSIZE;

        quota_fetch_inc(virtio_ctl, NETPKT_RX);

        if (first_netpkt == NULL)
        {
            first_netpkt = netpkt;
            pre_netpkt = first_netpkt;
        }
        else
        {
            pre_netpkt->next = netpkt;
            pre_netpkt = netpkt;
        }
    }

    if (first_netpkt)
    {
        ETH_DATA_TO_STACK(ethdev, first_netpkt);
    }

    /* 填充buffer，用于接收 */

    virtio_net_rxfill(ethdev);

    return 0;
}

static inline int virtio_net_set_macaddr(ETH_DEV *ethdev, const char *hwaddr)
{
    /* 上层改变协议栈地址即可 */
    return OK;
}

static int virtio_net_get_macaddr(ETH_DEV *ethdev, char *hwaddr)
{
    struct virtio_net_priv_s *virtio_ctl = (struct virtio_net_priv_s *)ethdev->dev_ctrl;
    struct virtio_device *vdev = virtio_ctl->vdev;

    /* 读取MAC地址 */
    virtio_read_config(vdev, offsetof(struct virtio_net_config_s, mac), hwaddr, IFHWADDRLEN);

    return OK;
}

/**
 *  设备ioctl示例
 */
static int virtio_net_ioctl(ETH_DEV *ethdev, int cmd, unsigned long arg)
{
    switch (cmd)
    {
    case SIOCSIFHWADDR:
        return virtio_net_set_macaddr(ethdev,
                                      (const char *)((struct ifreq *)arg)->ifr_hwaddr.sa_data);
    default:
        return -ENOTSUP;
    }
}

/****************************************************************************
 * Name: virtio_net_rxready
 ****************************************************************************/

static void virtio_net_rxready(struct virtqueue *vq)
{
    struct virtio_net_priv_s *virtio_ctl = (struct virtio_net_priv_s *)vq->vq_dev->priv;

    virtqueue_disable_cb_lock (vq, &virtio_ctl->spin_lock[VIRTIO_NET_RX]);

    eth_notify_rx_task (&virtio_ctl->ethdev);
}

/****************************************************************************
 * Name: virtio_net_txdone
 ****************************************************************************/

static void virtio_net_txdone(struct virtqueue *vq)
{
    struct virtio_net_priv_s *virtio_ctl = (struct virtio_net_priv_s *)vq->vq_dev->priv;

    virtqueue_disable_cb_lock (vq, &virtio_ctl->spin_lock[VIRTIO_NET_TX]);

    virtio_net_txfree (virtio_ctl);
}

/****************************************************************************
 * Name: virtio_net_init
 ****************************************************************************/

static int virtio_net_init(struct virtio_net_priv_s *virtio_ctl, struct virtio_device *vdev)
{
    const char *vqnames[VIRTIO_NET_NUM];
    vq_callback callbacks[VIRTIO_NET_NUM];
    uint16_t maxvqpair;
    int ret;

    virtio_ctl->vdev = vdev;
    vdev->priv = virtio_ctl;

    /* Initialize the virtio device */

    virtio_set_status(vdev, VIRTIO_CONFIG_STATUS_DRIVER);
    virtio_negotiate_features(vdev, 1 << VIRTIO_NET_F_MAC);
    virtio_set_status(vdev, VIRTIO_CONFIG_FEATURES_OK);

    vqnames[VIRTIO_NET_RX] = "virtio_net_rx";
    vqnames[VIRTIO_NET_TX] = "virtio_net_tx";
    callbacks[VIRTIO_NET_RX] = virtio_net_rxready;
    callbacks[VIRTIO_NET_TX] = virtio_net_txdone;

    ret = virtio_create_virtqueues(vdev, 0, VIRTIO_NET_NUM, vqnames, callbacks);
    if (ret < 0)
    {
        return ret;
    }

    virtio_set_status(vdev, VIRTIO_CONFIG_STATUS_DRIVER_OK);

    virtio_ctl->bufnum = MIN(vdev->vrings_info[VIRTIO_NET_TX].info.num_descs,
                             vdev->vrings_info[VIRTIO_NET_RX].info.num_descs);

    virtio_read_config(vdev, offsetof(struct virtio_net_config_s, max_virtqueue_pairs), &maxvqpair,
                       2);

    return OK;
}

/****************************************************************************
 * Name: virtio_net_probe
 ****************************************************************************/

static int virtio_net_probe(struct virtio_device *vdev)
{
    struct device *dev = &vdev->device;
    struct virtio_net_priv_s *virtio_ctl = NULL;
    int ret;

    /* 使用托管内存分配 */
    virtio_ctl = devm_kzalloc(dev, sizeof(struct virtio_net_priv_s), GFP_KERNEL);
    if (virtio_ctl == NULL)
        return -ENOMEM;

    spin_lock_init(&virtio_ctl->spin_lock[VIRTIO_NET_RX]);
    spin_lock_init(&virtio_ctl->spin_lock[VIRTIO_NET_TX]);

    ret = virtio_net_init(virtio_ctl, vdev);
    if (ret < 0)
        return ret;

    atomic_set(&virtio_ctl->quota[NETPKT_RX], virtio_ctl->bufnum);
    atomic_set(&virtio_ctl->quota[NETPKT_TX], virtio_ctl->bufnum);

    ret = eth_device_init(&virtio_net_drv_info, (void *)virtio_ctl, dev);
    if (ret != OK)
        return ret;

    netdev_set_link_up(virtio_ctl->ethdev.netdev);

    virtio_net_rxfill(&virtio_ctl->ethdev);

    return OK;
}

/****************************************************************************
 * Name: virtio_net_remove
 ****************************************************************************/

static void virtio_net_remove(struct virtio_device *vdev)
{
    struct virtio_net_priv_s *virtio_ctl = vdev->priv;

    /* 停止硬件 */
    virtio_reset_device(vdev);
    virtio_delete_virtqueues(vdev);

    /* 所有资源（内存）由 devres 自动释放 */
    return;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: virtio_register_net_driver
 ****************************************************************************/

int virtio_register_net_driver(void)
{
    return virtio_add_driver(&g_virtio_net_driver);
}
INIT_EXPORT_DRIVER(virtio_register_net_driver, "virtio mmio net driver");
