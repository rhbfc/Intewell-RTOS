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
#include <fcntl.h>
#include <list.h>
#include <net/ethernet_dev.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/net.h>
#include <net/netdev.h>
#include <net/netdev_ioctl.h>
#include <net/packet.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <netpacket/packet.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <ttos.h>
#include <ttosBase.h>
#include <ttos_init.h>
#include <restart.h>

#define PACKET_SOCKS_ARRSIZE 8

DEFINE_SPINLOCK(ACTIVE_PACKET_SOCKETS_LOCK);

struct active_packet_sockets ACTIVE_PACKET_SOCKETS = {.count = 0, .socks = NULL};

static int bind_active_packet_sockets(packet_sock_t *target)
{
    packet_sock_t **tmp_sockets_list;
    uint32_t insert_index = ACTIVE_PACKET_SOCKETS.count;
    int ret;
    int i;

    if (NULL == ACTIVE_PACKET_SOCKETS.socks)
    {
        ACTIVE_PACKET_SOCKETS.socks = calloc(sizeof(packet_sock_t *), PACKET_SOCKS_ARRSIZE);

        if (NULL == ACTIVE_PACKET_SOCKETS.socks)
        {
            return -ENOMEM;
        }

        ACTIVE_PACKET_SOCKETS.count = 0;
        ACTIVE_PACKET_SOCKETS.max_count = PACKET_SOCKS_ARRSIZE;
    }
    else
    {
        if (ACTIVE_PACKET_SOCKETS.count == ACTIVE_PACKET_SOCKETS.max_count)
        {
            tmp_sockets_list = ACTIVE_PACKET_SOCKETS.socks;

            ACTIVE_PACKET_SOCKETS.socks = calloc(
                sizeof(packet_sock_t), ACTIVE_PACKET_SOCKETS.max_count + PACKET_SOCKS_ARRSIZE);

            if (NULL == ACTIVE_PACKET_SOCKETS.socks)
            {
                ACTIVE_PACKET_SOCKETS.socks = tmp_sockets_list;
                return -ENOMEM;
            }
            else
            {
                memcpy(ACTIVE_PACKET_SOCKETS.socks, tmp_sockets_list,
                       sizeof(packet_sock_t *) * (size_t)ACTIVE_PACKET_SOCKETS.max_count);
                ACTIVE_PACKET_SOCKETS.max_count += PACKET_SOCKS_ARRSIZE;
                free(tmp_sockets_list);
            }
        }
    }

    ACTIVE_PACKET_SOCKETS.socks[insert_index] = target;
    ACTIVE_PACKET_SOCKETS.count += 1;

    return OK;
}

static void unbind_active_packet_sockets(packet_sock_t *target)
{
    packet_sock_t **tmp_sockets_list;
    int i;
    int found = 0;

    if (NULL == ACTIVE_PACKET_SOCKETS.socks)
    {
        return;
    }
    else
    {
        for (int i = 0; i < ACTIVE_PACKET_SOCKETS.count; i += 1)
        {
            if (ACTIVE_PACKET_SOCKETS.socks[i]->eth == target->eth &&
                ACTIVE_PACKET_SOCKETS.socks[i] == target)
            {
                found = 1;
            }

            if (found)
            {
                if (i < ACTIVE_PACKET_SOCKETS.count - 1)
                {
                    ACTIVE_PACKET_SOCKETS.socks[i] = ACTIVE_PACKET_SOCKETS.socks[i + 1];
                }
                else
                {
                    ACTIVE_PACKET_SOCKETS.socks[i] = 0;
                }
            }
        }

        if (found)
        {
            ACTIVE_PACKET_SOCKETS.count -= 1;
        }
    }

    if (ACTIVE_PACKET_SOCKETS.max_count > PACKET_SOCKS_ARRSIZE &&
        ACTIVE_PACKET_SOCKETS.count < ACTIVE_PACKET_SOCKETS.max_count / 4)
    {
        tmp_sockets_list = ACTIVE_PACKET_SOCKETS.socks;

        ACTIVE_PACKET_SOCKETS.socks =
            calloc(sizeof(packet_sock_t), ACTIVE_PACKET_SOCKETS.max_count / 2);

        if (NULL == ACTIVE_PACKET_SOCKETS.socks)
        {
            ACTIVE_PACKET_SOCKETS.socks = tmp_sockets_list;
        }
        else
        {
            ACTIVE_PACKET_SOCKETS.max_count = ACTIVE_PACKET_SOCKETS.max_count / 2;
            memcpy(ACTIVE_PACKET_SOCKETS.socks, tmp_sockets_list,
                   sizeof(packet_sock_t *) * (size_t)ACTIVE_PACKET_SOCKETS.max_count);
            free(tmp_sockets_list);
        }
    }
}

static int packet_setup(struct socket *psock)
{
    assert(psock);

    T_TTOS_ReturnCode ret;

    packet_sock_t *priv = calloc(1, sizeof(packet_sock_t));
    if (priv == NULL)
    {
        return -ENOMEM;
    }

    ret = TTOS_CreateSemaEx(0, &priv->pkt_sem);
    if (TTOS_OK != ret)
    {
        free(priv);
        return -ENOMEM;
    }

    priv->proto = psock->s_proto;
    priv->stat.kfd = NULL;
    priv->stat.aio.aio_enable = false;
    priv->stat.aio.pid = -1;
    priv->rcv_ticks = TTOS_SEMA_WAIT_FOREVER;
    priv->eth = NULL;

    TTOS_CreateMutex(1, 0, &priv->stat.mutex_lock);

    psock->s_priv = priv;

    return OK;
}

static sockcaps_t packet_sockcaps(struct socket *psock)
{
    return SOCKCAP_NONBLOCKING;
}

static int packet_bind(struct socket *psock, const struct sockaddr *addr, socklen_t addrlen)
{
    int ifindex;
    struct sockaddr_ll *sll;
    irq_flags_t pl_flags;
    int ret;

    assert(psock);

    packet_sock_t *priv = psock->s_priv;
    NET_DEV *dev;

    sll = (struct sockaddr_ll *)addr;

    if (sll->sll_family != AF_PACKET || addrlen != sizeof(struct sockaddr_ll))
    {
        return -EINVAL;
    }

    ifindex = ((struct sockaddr_ll *)addr)->sll_ifindex;

    dev = netdev_get_by_unit(ifindex);
    if (dev == NULL)
    {
        return -EADDRNOTAVAIL;
    }

    priv->eth = dev->link_data;

    spin_lock_irqsave(&ACTIVE_PACKET_SOCKETS_LOCK, pl_flags);

    ret = bind_active_packet_sockets(priv);

    spin_unlock_irqrestore(&ACTIVE_PACKET_SOCKETS_LOCK, pl_flags);

    return ret;
}

static void packet_addref(struct socket *psock) {}

static ssize_t pkt_read_remains(packet_sock_t *priv, void *buf, size_t buflen)
{
    size_t copylen = 0;
    ETH_NETPKT *__pkt;
    irq_flags_t rw_flags;
    int ret;

    ret = TTOS_ObtainMutex(priv->stat.mutex_lock, TTOS_WAIT_FOREVER);
    if (ret != TTOS_OK)
    {
        return -ttos_ret_to_errno(ret);
    }

    if (priv->pkt_list.head)
    {
        copylen = priv->pkt_list.head->len;
        if (copylen > buflen)
        {
            copylen = buflen;
        }
        memcpy(buf, priv->pkt_list.head->buf, copylen);

        __pkt = priv->pkt_list.head;
        priv->pkt_list.head = priv->pkt_list.head->next;
        eth_netpkt_free(__pkt);

        priv->stat.rx_event -= 1;

        if (priv->pkt_list.head == NULL)
        {
            priv->pkt_list.tail = NULL;
        }
    }

    TTOS_ReleaseMutex(priv->stat.mutex_lock);

    return (ssize_t)copylen;
}

static int packet_poll(struct socket *psock, struct kpollfd *kfds, bool setup)
{
    packet_sock_t *priv = psock->s_priv;
    irq_flags_t rw_flags;

    short eventset = kfds != NULL ? kfds->pollfd.revents : 0;

    if (setup)
    {
        priv->stat.kfd = kfds;
        kfds->priv = &priv->stat.kfd;

        if (priv->stat.rx_event > 0)
        {
            eventset |= POLLIN;
        }
        if (priv->stat.tx_event > 0)
        {
            eventset |= POLLOUT;
        }
        if (priv->stat.err_event > 0)
        {
            eventset |= POLLERR;
        }

        kpoll_notify(&kfds, 1, eventset, &priv->stat.aio);
    }
    else
    {
        priv->stat.kfd = NULL;
        *(struct kpollfd **)kfds->priv = NULL;
        kfds->priv = NULL;
    }

    return 0;
}

ssize_t packet_recvmsg(struct socket *psock, struct msghdr *msg, int flags)
{
    void *buf = msg->msg_iov->iov_base;
    size_t len = msg->msg_iov->iov_len;
    struct sockaddr_ll *from = msg->msg_name;
    socklen_t fromlen = msg->msg_namelen;
    packet_sock_t *priv = psock->s_priv;
    T_TTOS_ReturnCode sem_ret;
    ssize_t ret;

    if (priv->eth == NULL)
    {
        return -EADDRNOTAVAIL;
    }

    if ((from != NULL) && (fromlen != sizeof(struct sockaddr_ll)))
    {
        return -EINVAL;
    }

    ret = pkt_read_remains(priv, buf, len);

    if (ret == 0)
    {
        if (_SS_ISNONBLOCK(priv->stat.flags) || (flags & MSG_DONTWAIT))
        {
            ret = -EAGAIN;
        }
        else
        {
            sem_ret = TTOS_ObtainSema(priv->pkt_sem, priv->rcv_ticks);
            sem_ret = (sem_ret == TTOS_SIGNAL_INTR) ? EINTR : sem_ret;
            if (sem_ret == TTOS_OK)
            {
                ret = pkt_read_remains(priv, buf, len);
                assert(ret > 0);
            }
            else if (sem_ret == EINTR)
            {
                ret = -ERR_RESTART_IF_SIGNAL;
            }
            else if (sem_ret == TTOS_TIMEOUT || sem_ret == TTOS_UNSATISFIED)
            {
                ret = -EAGAIN;
            }
            else if (sem_ret != TTOS_OK)
            {
                ret = -EBUSY;
            }
        }
    }
    else
    {
        sem_ret = TTOS_ObtainSemaUninterruptable(priv->pkt_sem, 0);
        assert(sem_ret == TTOS_UNSATISFIED || sem_ret == TTOS_OK);
    }

    if (from != NULL)
    {
        from->sll_family = AF_PACKET;
        from->sll_hatype = ARPHRD_ETHER;
        from->sll_halen = IFHWADDRLEN;

        if (len >= sizeof(ether_header_t))
        {
            from->sll_protocol = ((ether_header_t *)buf)->etype;

            bcopy(((ether_header_t *)buf)->src, from->sll_addr, IFHWADDRLEN);
        }
    }

    return ret;
}

static ssize_t packet_sendmsg(struct socket *psock, struct msghdr *msg, int flags)
{
    ETH_NETPKT *pkt;
    NET_DEV *from_dev;
    ETH_DEV *from_eth;
    packet_sock_t *priv = psock->s_priv;
    void *buf = msg->msg_iov->iov_base;
    size_t len = msg->msg_iov->iov_len;
    struct sockaddr_ll *to = msg->msg_name;
    socklen_t tolen = msg->msg_namelen;
    long send_flag;
    int ret;
#ifdef CONFIG_SUPPORT_PCAP_TOOL
    struct pcap_nsock *pnsock;
    char tbuf[1600];
#endif

    if (priv->eth == NULL && to == NULL)
    {
        return -ENODEV;
    }

    /* 用户态使用sendto()时检查协议族 */
    if ((to != NULL) && ((tolen != sizeof(struct sockaddr_ll)) || (to->sll_family != AF_PACKET)))
    {
        return -EAFNOSUPPORT;
    }

    if (NULL == priv->eth)
    {
        from_dev = netdev_get_by_unit(to->sll_ifindex);
        if (NULL == from_dev)
        {
            return -ENODEV;
        }
        from_eth = from_dev->link_data;
    }
    else
    {
        from_eth = priv->eth;
        from_dev = from_eth->netdev;
    }

    pkt = eth_netpkt_alloc(0);
    if (NULL == pkt)
    {
        return -ENOMEM;
    }

    pkt->buf = (unsigned char *)buf;
    pkt->len = len;

#ifdef CONFIG_SUPPORT_PCAP_TOOL
    if (TTOS_OK == TTOS_ObtainMutex(PCAP_SOCKLIST_MUTEX, TTOS_MUTEX_WAIT_FOREVER))
    {
        list_for_each_entry(pnsock, &PCAP_NSOCK_LIST, node)
        {
            if (from_eth == pnsock->eth)
            {
                memcpy(tbuf, buf, len);

                TTOS_SendMsgq(pnsock->msgq, tbuf, len, 0, 0);
            }
        }
        TTOS_ReleaseMutex(PCAP_SOCKLIST_MUTEX);
    }
#endif

    // spin_lock_irqsave(&from_eth->send_lock, send_flag);
    ret = from_eth->drv_info->eth_func->send(from_eth, pkt);
    // spin_unlock_irqrestore(&from_eth->send_lock, send_flag);

    eth_netpkt_free(pkt);

    if (ret == OK)
    {
        atomic64_inc(&from_dev->ll_info.ethernet.afpkt_stats.out_pacs);
        atomic64_add(&from_dev->ll_info.ethernet.afpkt_stats.out_bytes, len);
        return (ssize_t)len;
    }
    else
    {
        atomic64_inc(&from_dev->ll_info.ethernet.afpkt_stats.eout_pacs);
        atomic64_add(&from_dev->ll_info.ethernet.afpkt_stats.eout_bytes, len);
        return ret;
    }
}

static int packet_close(struct socket *psock)
{
    assert(psock);

    packet_sock_t *priv = psock->s_priv;
    irq_flags_t pl_flags, rw_flags;
    ETH_NETPKT *pkt, *__cpkt;

    T_TTOS_ReturnCode ret;

    spin_lock_irqsave(&ACTIVE_PACKET_SOCKETS_LOCK, pl_flags);
    unbind_active_packet_sockets(priv);
    spin_unlock_irqrestore(&ACTIVE_PACKET_SOCKETS_LOCK, pl_flags);

    ret = TTOS_DeleteSema(priv->pkt_sem);

try_again:
    ret = TTOS_ObtainMutex(priv->stat.mutex_lock, TTOS_WAIT_FOREVER);
    if (ret != TTOS_OK)
    {
        goto try_again;
    }

    pkt = priv->pkt_list.head;
    while (pkt)
    {
        __cpkt = pkt;
        pkt = pkt->next;
        eth_netpkt_free(__cpkt);
    }

    TTOS_ReleaseMutex(priv->stat.mutex_lock);

    TTOS_DeleteMutex(priv->stat.mutex_lock);

    free(priv);

    return OK;
}

static int packet_ioctl(struct socket *psock, unsigned int cmd, unsigned long arg)
{
    struct ifreq *ifr;
    packet_sock_t *priv = psock->s_priv;
    NET_DEV *ndev;
    int argval;

    switch (cmd)
    {
    case FIONBIO:
        argval = *(int *)arg;
        if (argval)
        {
            priv->stat.flags |= _SF_NONBLOCK;
        }
        else
        {
            priv->stat.flags &= (~_SF_NONBLOCK);
        }
        return OK;
    case SIOCGIFINDEX:
        ifr = (struct ifreq *)arg;
        ndev = netdev_get_by_name(ifr->ifr_name);
        if (ndev)
        {
            ifr->ifr_ifindex = ndev->netdev_unit;
            return OK;
        }
        else
        {
            return -ENODEV;
        }
    case SIOCGIFFLAGS:
        ifr = (struct ifreq *)arg;
        ndev = netdev_get_by_name(ifr->ifr_name);
        if (ndev)
        {
            ifr->ifr_flags = netdev_flags_to_posix_flags(ndev);
            return OK;
        }
        else
        {
            return -ENODEV;
        }
    case SIOCGIFHWADDR:
        return ndev_ioctl_get_hwaddr_by_name((struct ifreq *)arg);
    default:
        break;
    }

    return OK;
}

static int packet_getsockopt(struct socket *psock, int level, int option, void *value,
                             socklen_t *value_len)
{
    return 0;
}

static int packet_setsockopt(struct socket *psock, int level, int option, const void *value,
                             socklen_t value_len)
{
    packet_sock_t *priv = psock->s_priv;
    struct timeval *timeout;

    if (level != SOL_SOCKET)
    {
        return -ENOTSUP;
    }

    switch (option)
    {
    case SO_RCVTIMEO:
        timeout = (struct timeval *)value;
        uint32_t ticks = (uint32_t)((uint64_t)(1000000 * timeout->tv_sec + timeout->tv_usec) *
                                    (uint64_t)TTOS_GetSysClkRate() / 1000000UL);
        priv->rcv_ticks = ticks;
        break;

    default:
        break;
    }

    return OK;
}

static const struct sa_type packet_supp_sa_types[] = {
    {.family = AF_PACKET, .type = SOCK_RAW, .protocol = 0},
};

static struct sock_intf_s packet_psocket_ops = {
    .type = packet_supp_sa_types,
    .type_count = array_size(packet_supp_sa_types),
    .si_setup = packet_setup,
    .si_sockcaps = packet_sockcaps,
    .si_addref = packet_addref,
    .si_bind = packet_bind,
    .si_getsockname = NULL,
    .si_getpeername = NULL,
    .si_listen = NULL,
    .si_connect = NULL,
    .si_accept = NULL,
    .si_poll = packet_poll,
    .si_sendmsg = packet_sendmsg,
    .si_recvmsg = packet_recvmsg,
    .si_close = packet_close,
    .si_ioctl = packet_ioctl,
    .si_socketpair = NULL,
    .si_shutdown = NULL,
    .si_getsockopt = NULL,
    .si_setsockopt = packet_setsockopt,
    .si_sendfile = NULL,
};

int register_packet_sockif()
{
    return register_sockif(&packet_psocket_ops);
}
INIT_EXPORT_SUBSYS(register_packet_sockif, "init packet socket interface");
