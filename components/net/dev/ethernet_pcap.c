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

#include <assert.h>
#include <errno.h>
#include <fs/kpoll.h>
#include <net/net.h>
#include <netinet/in.h>
#include <net/if.h>
#include <poll.h>
#include <fcntl.h>
#include <net/netdev.h>
#include <net/netdev_ioctl.h>
#include <net/ethernet_dev.h>
#include <net/if_arp.h>
#include <ttosBase.h>
#include <uaccess.h>

#undef KLOG_TAG
#define KLOG_TAG "PCAP_PSOCK"
#include <klog.h>

/* 抓包工具使用的自定义套接字，临时方案 */
#define AF_PCAP 128

int pcap_enable_flag = 0;

#ifdef CONFIG_SUPPORT_PCAP_TOOL

static const struct sa_type eth_pcap_supp_sa_types[] =
{
    {.family = AF_PCAP, .type = SOCK_RAW, .protocol = IPPROTO_IP},
};

static int try_delete_msgq_and_node(MSGQ_ID msgq, struct list_node *node)
{
    int ret = OK;

    if (msgq != NULL)
    {
        ret = TTOS_DeleteMsgq((MSGQ_ID)msgq);

        if (ret == TTOS_OK)
        {
            list_delete(node);
        }
    }

    return ret;
}

int if_packet_setup (struct netdev_sock *nsock)
{
    struct pcap_nsock *pnsock = calloc(1, sizeof(struct pcap_nsock));

    if (pnsock == NULL)
    {
        return -ENOMEM;
    }

    pnsock->nsock = nsock;
    pnsock->msgq = NULL;
    pnsock->eth = NULL;
    nsock->priv = (void *)pnsock;

    return OK;
}

ssize_t if_packet_recvmsg (struct netdev_sock *nsock, struct msghdr *msg, int flags)
{
    T_UWORD msglen = 0;
    char buf[1600];
    T_TTOS_ReturnCode ttosret;
    int ret;

    struct pcap_nsock *pnsock = (struct pcap_nsock *)nsock->priv;

    if (pnsock->msgq == NULL)
    {
        return (ssize_t)-EINVAL;
    }

    ttosret = TTOS_ReceiveMsgq(pnsock->msgq,
        buf, &msglen,
        0, TTOS_MSGQ_WAIT_FOREVER);
    ret = -ttos_ret_to_errno(ttosret);

    if (0 == ret)
    {
        ret = copy_to_user(msg->msg_iov->iov_base, buf, msglen);
        if (0 == ret)
        {
            msg->msg_iov->iov_len = msglen;
            msg->msg_iovlen = 1;
        }
    }

    return ret == 0 ? (ssize_t)msglen : (ssize_t)ret;
}

int if_packet_close (struct netdev_sock *nsock)
{
    int ret = OK;
    irq_flags_t flags;

    struct pcap_nsock *pnsock = (struct pcap_nsock *)nsock->priv;

    TTOS_ObtainMutex(PCAP_SOCKLIST_MUTEX, TTOS_MUTEX_WAIT_FOREVER);
    ret = try_delete_msgq_and_node(pnsock->msgq, &pnsock->node);
    if (ret == OK)
    {
        free(pnsock);
    }
    TTOS_ReleaseMutex(PCAP_SOCKLIST_MUTEX);

    return ret;
}

int if_pcaket_ioctl (struct netdev_sock *nsock, int cmd, unsigned long arg)
{
    struct ifreq *ifr = (struct ifreq *)arg;
    pcap_ioctl_t *pcap = (pcap_ioctl_t *)ifr->ifr_data;
    NET_DEV *ndev;
    struct pcap_nsock *pnsock;
    MSGQ_ID msgq;
    irq_flags_t flags;
    T_TTOS_ReturnCode mutex_ret;

    if (cmd != SIOCXPCAP)
    {
        return -ENOTSUP;
    }

    if (pcap == NULL || nsock->priv == NULL)
        return -EINVAL;

    pnsock = (struct pcap_nsock *)nsock->priv;

   ndev = netdev_get_by_name (ifr->ifr_name);
    if (NULL == ndev)
    {
        return -ENODEV;
    }

    switch (pcap->op)
    {
    case PCAP_OP_START:
        if (pnsock->msgq != NULL)
        {
            return -EINVAL;
        }

        if (pnsock == NULL)
        {
            return -ENOMEM;
        }

        int ret = TTOS_CreateMsgqEx(1600, CONFIG_PCAP_CACHEABLE_ENTRIES, &msgq);

        if (ret < 0)
        {
            return ret;
        }

        pnsock->msgq = msgq;
        pnsock->eth = (ETH_DEV *)ndev->link_data;
        mutex_ret = TTOS_ObtainMutex(PCAP_SOCKLIST_MUTEX, TTOS_MUTEX_WAIT_FOREVER);
        list_add(&pnsock->node, &PCAP_NSOCK_LIST);
        TTOS_ReleaseMutex(PCAP_SOCKLIST_MUTEX);
        break;

    case PCAP_OP_STOP:
        if (pnsock->msgq != NULL)
        {
            TTOS_ObtainMutex(PCAP_SOCKLIST_MUTEX, TTOS_MUTEX_WAIT_FOREVER);
            ret = try_delete_msgq_and_node(pnsock->msgq, &pnsock->node);
            pnsock->msgq = NULL;
            TTOS_ReleaseMutex(PCAP_SOCKLIST_MUTEX);
            if(ret < 0)
            {
                return ret;
            }
        }
        else
        {
            return -EINVAL;
        }
        break;

    default:
        return -ENOTSUP;
    }

    return OK;
}

static struct netdev_sock_ops eth_pcap_ndev_ops =
{
    .ns_setup       = if_packet_setup,
    .ns_sockcaps    = NULL,
    .ns_addref      = NULL,
    .ns_bind        = NULL,
    .ns_getsockname = NULL,
    .ns_getpeername = NULL,
    .ns_listen      = NULL,
    .ns_connect     = NULL,
    .ns_accept      = NULL,
    .ns_poll        = NULL,
    .ns_sendmsg     = NULL,
    .ns_recvmsg     = if_packet_recvmsg,
    .ns_close       = if_packet_close,
    .ns_ioctl       = if_pcaket_ioctl,
    .ns_socketpair  = NULL,
    .ns_shutdown    = NULL,
    .ns_getsockopt  = NULL,
    .ns_setsockopt  = NULL,
    .ns_sendfile    = NULL,
};

void eth_pcap_register_sockif()
{
    T_TTOS_ReturnCode ret;

    ret = TTOS_CreateMutex(1, 0, &PCAP_SOCKLIST_MUTEX);
    if (TTOS_OK != ret)
    {
        KLOG_E("Can't create pcap mutex");
        PCAP_SOCKLIST_MUTEX = NULL;

        return;
    }

    if (netdev_register_net_stack_sockif(eth_pcap_supp_sa_types, array_size(eth_pcap_supp_sa_types), &eth_pcap_ndev_ops) != 0)
    {
        KLOG_E("Add Ethernet AF_PCAP Socket Interfaces Failed");
    }
    else
    {
        KLOG_I("Register Ethernet AF_PCAP Socket Interfaces");
    }
}
#endif
