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

#include <net/netdev.h>
#include <assert.h>
#include <errno.h>
#include <fs/kpoll.h>
#include <sys/ioctl.h>
#include <net/net.h>
#include <netinet/in.h>
#include <poll.h>
#include <net/if.h>
#include <ttos_init.h>
#include <spinlock.h>
#include <net/netdev_ioctl.h>

#define KLOG_TAG "NETDEV_PSOCK"
#include <klog.h>

/* PUBLIC */

static LIST_HEAD (G_NDEV_SOCKIF);
static DEFINE_SPINLOCK (NDEV_SOCKIF_LOCK);

static struct netdev_sock_if *match_sa_type (const struct sock_intf_s *psock_sockif);

static int nsock_setup (struct socket *psock)
{
    assert (psock);

    int ret;
    struct netdev_sock_if *ndev_sockif;

    ndev_sockif = match_sa_type (psock->s_sockif);
    if (NULL == ndev_sockif)
    {
        return -ENOTSUP;
    }

    struct netdev_sock *nsock = malloc (sizeof (struct netdev_sock));
    if (nsock == NULL)
    {
        return -ENOMEM;
    }

    nsock->satype.family = psock->s_domain;
    nsock->satype.protocol = psock->s_proto;
    nsock->satype.type = psock->s_type;
    nsock->sockif = ndev_sockif;
    psock->s_priv = nsock;

    if (nsock->sockif->ops->ns_setup == NULL)
    {
        free (nsock);
        return -ENOTSUP;
    }

    ret = nsock->sockif->ops->ns_setup (nsock);
    if (ret < 0)
    {
        free (nsock);
    }

    return ret;
}

static sockcaps_t nsock_sockcaps (struct socket *psock)
{
    assert (psock);

    struct netdev_sock *nsock = (struct netdev_sock *)psock->s_priv;

    if (nsock->sockif->ops->ns_sockcaps == NULL)
    {
        return -ENOTSUP;
    }

    return nsock->sockif->ops->ns_sockcaps (nsock);
}

static int nsock_bind (struct socket *psock, const struct sockaddr *addr, socklen_t addrlen)
{
    assert (psock);

    struct netdev_sock *nsock = (struct netdev_sock *)psock->s_priv;

    if (nsock->sockif->ops->ns_bind == NULL)
    {
        return -ENOTSUP;
    }

    return nsock->sockif->ops->ns_bind (nsock, addr, addrlen);
}

static int nsock_getsockname (struct socket *psock, struct sockaddr *addr, socklen_t *addrlen)
{
    assert (psock);

    struct netdev_sock *nsock = (struct netdev_sock *)psock->s_priv;

    if (nsock->sockif->ops->ns_getsockname == NULL)
    {
        return -ENOTSUP;
    }

    return nsock->sockif->ops->ns_getsockname (nsock, addr, addrlen);
}

static int nsock_getpeername (struct socket *psock, struct sockaddr *addr, socklen_t *addrlen)
{
    assert (psock);

    struct netdev_sock *nsock = (struct netdev_sock *)psock->s_priv;

    if (nsock->sockif->ops->ns_getpeername == NULL)
    {
        return -ENOTSUP;
    }

    return nsock->sockif->ops->ns_getpeername (nsock, addr, addrlen);
}

static int nsock_listen (struct socket *psock, int backlog)
{
    assert (psock);

    struct netdev_sock *nsock = (struct netdev_sock *)psock->s_priv;

    if (nsock->sockif->ops->ns_listen == NULL)
    {
        return -ENOTSUP;
    }

    return nsock->sockif->ops->ns_listen (nsock, backlog);
}

static int nsock_connect (struct socket *psock, const struct sockaddr *addr, socklen_t addrlen)
{
    assert (psock);

    struct netdev_sock *nsock = (struct netdev_sock *)psock->s_priv;

    if (nsock->sockif->ops->ns_connect == NULL)
    {
        return -ENOTSUP;
    }

    return nsock->sockif->ops->ns_connect (nsock, addr, addrlen);
}

static int nsock_accept (struct socket *psock, struct sockaddr *addr, socklen_t *addrlen, struct socket *newsock, int flags)
{
    assert (psock);

    int ret;

    struct netdev_sock *nsock = (struct netdev_sock *)psock->s_priv;
    struct netdev_sock *new_nsock = (struct netdev_sock *)calloc (1, sizeof (struct netdev_sock));

    if (new_nsock == NULL)
    {
        return -ENOMEM;
    }

    if (nsock->sockif->ops->ns_accept == NULL)
    {
        free (new_nsock);
        return -ENOTSUP;
    }

    newsock->s_domain = psock->s_domain;
    newsock->s_type = psock->s_type;
    newsock->s_proto = psock->s_proto;
    newsock->s_sockif = psock->s_sockif;
    newsock->s_priv = new_nsock;

    ret = nsock->sockif->ops->ns_accept (nsock, addr, addrlen, new_nsock, flags);
    if (ret < 0)
    {
        free (new_nsock);
    }

    return ret;
}

static int nsock_poll (struct socket *psock, struct kpollfd *kfds, bool setup)
{
    assert (psock);

    struct netdev_sock *nsock = (struct netdev_sock *)psock->s_priv;

    if (nsock->sockif->ops->ns_poll == NULL)
    {
        return -ENOTSUP;
    }

    return nsock->sockif->ops->ns_poll (nsock, kfds, setup);
}

static ssize_t nsock_sendmsg (struct socket *psock, struct msghdr *msg, int flags)
{
    assert (psock);

    struct netdev_sock *nsock = (struct netdev_sock *)psock->s_priv;

    if (nsock->sockif->ops->ns_sendmsg == NULL)
    {
        return -ENOTSUP;
    }

    return nsock->sockif->ops->ns_sendmsg (nsock, msg, flags);
}

static ssize_t nsock_recvmsg (struct socket *psock, struct msghdr *msg, int flags)
{
    assert (psock);

    struct netdev_sock *nsock = (struct netdev_sock *)psock->s_priv;

    if (nsock->sockif->ops->ns_recvmsg == NULL)
    {
        return -ENOTSUP;
    }

    return nsock->sockif->ops->ns_recvmsg (nsock, msg, flags);
}

static void nsock_addref (struct socket *psock)
{
    /**
     * DO NOTHING
     *
     * sock 引用依靠底层 vfs inode 的引用计数
     * 接口无需对 sock 引用变化进行操作
     * 因此留空函数兼容
     */
}

static int nsock_close (struct socket *psock)
{
    assert (psock);

    int ret;

    struct netdev_sock *nsock = (struct netdev_sock *)psock->s_priv;

    if (nsock->sockif->ops->ns_close == NULL)
    {
        return -ENOTSUP;
    }

    ret = nsock->sockif->ops->ns_close (nsock);

    free (nsock);

    return ret;
}

static int nsock_ioctl (struct socket *psock, unsigned int cmd, unsigned long arg)
{
    assert (psock);

    int ret;

    struct netdev_sock *nsock = (struct netdev_sock *)psock->s_priv;

    struct ifreq *ifr = (struct ifreq *)arg;

    switch (cmd)
    {
        case SIOCGIFCONF:
            ret = ndev_ioctl_get_ifconf ((struct ifconf *)arg);
            break;
        case SIOCGIFMTU:
            ret = ndev_ioctl_get_mtu_by_name (ifr);
            break;
        case SIOCGIFHWADDR:
            ret = ndev_ioctl_get_hwaddr_by_name (ifr);
            break;
        case SIOCGIFFLAGS:
            ret = ndev_ioctl_get_flags_by_name (ifr);
            break;
        case SIOCGIFADDR:
            ret = ndev_ioctl_get_ip_by_name (ifr);
            break;
        case SIOCGIFBRDADDR:
            ret = ndev_ioctl_get_bcast_by_name (ifr);
            break;
        case SIOCGIFNETMASK:
            ret = ndev_ioctl_get_nmask_by_name (ifr);
            break;
        case SIOCSIFHWADDR:
            ret = ndev_ioctl_set_hwaddr_by_name (ifr);
            break;
        case SIOCSIFMTU:
            ret = ndev_ioctl_set_mtu_by_name (ifr);
            break;
        case SIOCSIFFLAGS:
            ret = ndev_ioctl_set_flags_by_name (ifr);
            break;
        case SIOCSIFADDR:
            ret = ndev_ioctl_set_ip_by_name (ifr);
            break;
        case SIOCSIFNETMASK:
            ret = ndev_ioctl_set_nmask_by_name (ifr);
            break;
        case SIOCGIFNAME:
            ret = ndev_ioctl_index_to_name (ifr);
            break;
        case SIOCGIFINDEX:
            ret = ndev_ioctl_name_to_index (ifr);
            break;
        case SIOCADDRT:
            ret = ndev_ioctl_add_route ((struct rtentry *)arg);
            break;
        case SIOCDELRT:
            ret = ndev_ioctl_del_route ((struct rtentry *)arg);
            break;
        case SIOCSIFBRDADDR:
            /* 设置网卡广播地址无意义，返回成功 */
            return 0;
        case SIOCGIFALLIPS:
            ret = ndev_ioctl_get_if_all_ips((netif_ips_t *)arg);
            break;
        case SIOCAIFADDR:
            ret = ndev_ioctl_add_ip_by_name(ifr);
            break;
        case SIOCDIFADDR:
            ret = ndev_ioctl_del_ip_by_name(ifr);
            break;
        case SIOCGMIIREG:
            ret = ndev_ioctl_read_mii(ifr);
            break;
        case SIOCSMIIREG:
            ret = ndev_ioctl_write_mii(ifr);
            break;
        case SIOCGAFPKTSTATS:
            ret = ndev_ioctl_get_afpacket_stats(ifr);
            break;
        default:
            if (nsock->sockif->ops->ns_ioctl == NULL)
            {
                ret = -ENOTSUP;
                break;
            }
            ret = nsock->sockif->ops->ns_ioctl (nsock, cmd, arg);
            break;
    }

    return ret;
}

static int nsock_socketpair (struct socket *psocks[2])
{
    assert (psocks);

    struct netdev_sock *nsocks[2] = { 0 };

    nsocks[0] = (struct netdev_sock *)psocks[0]->s_priv;
    nsocks[1] = (struct netdev_sock *)psocks[1]->s_priv;

    if (nsocks[0]->sockif->ops->ns_socketpair == NULL)
    {
        return -ENOTSUP;
    }

    return nsocks[0]->sockif->ops->ns_socketpair (nsocks);
}

static int nsock_shutdown (struct socket *psock, int how)
{
    assert (psock);

    struct netdev_sock *nsock = (struct netdev_sock *)psock->s_priv;

    if (nsock->sockif->ops->ns_shutdown == NULL)
    {
        return -ENOTSUP;
    }

    return nsock->sockif->ops->ns_shutdown (nsock, how);
}

static int nsock_getsockopt (struct socket *psock, int level, int option, void *value, socklen_t *value_len)
{
    assert (psock);

    struct netdev_sock *nsock = (struct netdev_sock *)psock->s_priv;

    if (nsock->sockif->ops->ns_getsockopt == NULL)
    {
        return -ENOTSUP;
    }

    return nsock->sockif->ops->ns_getsockopt (nsock, level, option, value, value_len);
}

static int nsock_setsockopt (struct socket *psock, int level, int option, const void *value, socklen_t value_len)
{
    assert (psock);

    struct netdev_sock *nsock = (struct netdev_sock *)psock->s_priv;

    if (nsock->sockif->ops->ns_setsockopt == NULL)
    {
        return -ENOTSUP;
    }

    return nsock->sockif->ops->ns_setsockopt (nsock, level, option, value, value_len);
}

static ssize_t nsock_sendfile (struct socket *psock, struct file *infile, off_t *offset, size_t count)
{
    assert (psock);

    struct netdev_sock *nsock = (struct netdev_sock *)psock->s_priv;

    if (nsock->sockif->ops->ns_sendfile == NULL)
    {
        return -ENOTSUP;
    }

    return nsock->sockif->ops->ns_sendfile (nsock, infile, offset, count);
}

static struct sock_intf_s *ndev_register_sockif(const struct sa_type *supp_types, const size_t supp_types_count)
{
    struct sock_intf_s *newif = NULL;

    newif = malloc(sizeof(struct sock_intf_s));
    if (NULL == newif)
    {
        return NULL;
    }

    newif->type           = supp_types;
    newif->type_count     = (int)supp_types_count;
    newif->si_setup       = nsock_setup;
    newif->si_sockcaps    = nsock_sockcaps;
    newif->si_addref      = nsock_addref;
    newif->si_bind        = nsock_bind;
    newif->si_getsockname = nsock_getsockname;
    newif->si_getpeername = nsock_getpeername;
    newif->si_listen      = nsock_listen;
    newif->si_connect     = nsock_connect;
    newif->si_accept      = nsock_accept;
    newif->si_poll        = nsock_poll;
    newif->si_sendmsg     = nsock_sendmsg;
    newif->si_recvmsg     = nsock_recvmsg;
    newif->si_close       = nsock_close;
    newif->si_ioctl       = nsock_ioctl;
    newif->si_socketpair  = nsock_socketpair;
    newif->si_shutdown    = nsock_shutdown;
    newif->si_getsockopt  = nsock_getsockopt;
    newif->si_setsockopt  = nsock_setsockopt;
    newif->si_sendfile    = nsock_sendfile;

    if (register_sockif (newif) == 0)
    {
        return newif;
    }
    else
    {
        free(newif);
        return NULL;
    }
}


/**
 * 由协议栈向netdev注册新的私有socket操作
 */
int netdev_register_net_stack_sockif (const struct sa_type *types, const size_t types_count, const struct netdev_sock_ops *ops)
{
    long flags;

    struct sock_intf_s *psock_sockif = ndev_register_sockif(types, types_count);
    if (NULL == psock_sockif)
    {
        return -1;
    }

    struct netdev_sock_if *ndev_sockif = (struct netdev_sock_if *)calloc (1, sizeof (struct netdev_sock_if));
    if (NULL == ndev_sockif)
    {
        return -1;
    }

    ndev_sockif->psock_sockif = psock_sockif;
    ndev_sockif->ops = ops;

    spin_lock_irqsave (&NDEV_SOCKIF_LOCK, flags);
    list_add (&ndev_sockif->list, &G_NDEV_SOCKIF);
    spin_unlock_irqrestore (&NDEV_SOCKIF_LOCK, flags);

    return 0;
}

static struct netdev_sock_if *match_sa_type (const struct sock_intf_s *psock_sockif)
{
    struct netdev_sock_if *sif;
    long flags;
    int errlevel = 0;

    spin_lock_irqsave (&NDEV_SOCKIF_LOCK, flags);

    list_for_each_entry (sif, &G_NDEV_SOCKIF, list)
    {
        if (sif->psock_sockif == psock_sockif)
        {
            spin_unlock_irqrestore (&NDEV_SOCKIF_LOCK, flags);
            return sif;
        }
    }

    spin_unlock_irqrestore (&NDEV_SOCKIF_LOCK, flags);

    return NULL;
}
