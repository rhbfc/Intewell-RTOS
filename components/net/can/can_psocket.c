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
#include <fs/kpoll.h>
#include <klog.h>
#include <list.h>
#include <net/can_dev.h>
#include <net/if.h>
#include <net/netdev.h>
#include <net/netdev_ioctl.h>
#include <netinet/in.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <ttos.h>
#include <ttosBase.h>
#include <ttos_init.h>

#include <driver/can/can.h>
#include <net/af_can.h>

#undef KLOG_TAG
#define KLOG_TAG "AF_CAN"
#include <klog.h>

#define CAN_SOCKS_ARRSIZE 8

DEFINE_SPINLOCK(ACTIVE_CAN_SOCKETS_LOCK);

struct active_can_sockets ACTIVE_CAN_SOCKETS = {.count = 0, .socks = NULL};

static int bind_active_can_sockets(can_sock_t *target)
{
    can_sock_t **tmp_sockets_list;
    uint32_t insert_index = ACTIVE_CAN_SOCKETS.count;
    int ret;
    int i;

    if (NULL == ACTIVE_CAN_SOCKETS.socks)
    {
        ACTIVE_CAN_SOCKETS.socks = calloc(sizeof(can_sock_t *), CAN_SOCKS_ARRSIZE);

        if (NULL == ACTIVE_CAN_SOCKETS.socks)
        {
            return -ENOMEM;
        }

        ACTIVE_CAN_SOCKETS.count = 0;
        ACTIVE_CAN_SOCKETS.max_count = CAN_SOCKS_ARRSIZE;
    }
    else
    {
        if (ACTIVE_CAN_SOCKETS.count == ACTIVE_CAN_SOCKETS.max_count)
        {
            tmp_sockets_list = ACTIVE_CAN_SOCKETS.socks;

            ACTIVE_CAN_SOCKETS.socks =
                calloc(sizeof(can_sock_t), ACTIVE_CAN_SOCKETS.max_count + CAN_SOCKS_ARRSIZE);

            if (NULL == ACTIVE_CAN_SOCKETS.socks)
            {
                ACTIVE_CAN_SOCKETS.socks = tmp_sockets_list;
                return -ENOMEM;
            }
            else
            {
                memcpy(ACTIVE_CAN_SOCKETS.socks, tmp_sockets_list,
                       sizeof(can_sock_t *) * (size_t)ACTIVE_CAN_SOCKETS.max_count);
                ACTIVE_CAN_SOCKETS.max_count += CAN_SOCKS_ARRSIZE;
                free(tmp_sockets_list);
            }
        }
    }

    ACTIVE_CAN_SOCKETS.socks[insert_index] = target;
    ACTIVE_CAN_SOCKETS.count += 1;

    return OK;
}

static void unbind_active_can_sockets(can_sock_t *target)
{
    can_sock_t **tmp_sockets_list;
    int i;
    int found = 0;

    if (NULL == ACTIVE_CAN_SOCKETS.socks)
    {
        return;
    }
    else
    {
        for (int i = 0; i < ACTIVE_CAN_SOCKETS.count; i += 1)
        {
            if (ACTIVE_CAN_SOCKETS.socks[i]->candev == target->candev &&
                ACTIVE_CAN_SOCKETS.socks[i] == target)
            {
                found = 1;
            }

            if (found)
            {
                if (i < ACTIVE_CAN_SOCKETS.count - 1)
                {
                    ACTIVE_CAN_SOCKETS.socks[i] = ACTIVE_CAN_SOCKETS.socks[i + 1];
                }
                else
                {
                    ACTIVE_CAN_SOCKETS.socks[i] = 0;
                }
            }
        }

        if (found)
        {
            ACTIVE_CAN_SOCKETS.count -= 1;
        }
    }

    if (ACTIVE_CAN_SOCKETS.max_count > CAN_SOCKS_ARRSIZE &&
        ACTIVE_CAN_SOCKETS.count < ACTIVE_CAN_SOCKETS.max_count / 4)
    {
        tmp_sockets_list = ACTIVE_CAN_SOCKETS.socks;

        ACTIVE_CAN_SOCKETS.socks = calloc(sizeof(can_sock_t), ACTIVE_CAN_SOCKETS.max_count / 2);

        if (NULL == ACTIVE_CAN_SOCKETS.socks)
        {
            ACTIVE_CAN_SOCKETS.socks = tmp_sockets_list;
        }
        else
        {
            ACTIVE_CAN_SOCKETS.max_count = ACTIVE_CAN_SOCKETS.max_count / 2;
            memcpy(ACTIVE_CAN_SOCKETS.socks, tmp_sockets_list,
                   sizeof(can_sock_t *) * (size_t)ACTIVE_CAN_SOCKETS.max_count);
            free(tmp_sockets_list);
        }
    }
}

static int can_setup(struct netdev_sock *nsock)
{
    T_TTOS_ReturnCode ret;

    can_sock_t *priv = calloc(1, sizeof(can_sock_t));
    if (priv == NULL)
    {
        return -ENOMEM;
    }

    priv->proto = nsock->satype.protocol;
    priv->candev = NULL;

    nsock->priv = priv;

    return OK;
}

static int can_bind(struct netdev_sock *nsock, const struct sockaddr *addr, socklen_t addrlen)
{
    NET_DEV *netdev;
    int ifindex;
    struct sockaddr_can *saddr;
    irq_flags_t pl_flags;
    int fs_fd;
    int ret;

    assert(nsock);

    can_sock_t *priv = nsock->priv;

    saddr = (struct sockaddr_can *)addr;

    if (saddr->can_family != AF_CAN || addrlen != sizeof(struct sockaddr_can))
    {
        return -EINVAL;
    }

    ifindex = ((struct sockaddr_can *)addr)->can_ifindex;

    netdev = netdev_get_by_unit(ifindex);
    if (netdev == NULL || netdev->ll_type != NET_LL_CAN)
    {
        return -EADDRNOTAVAIL;
    }

    priv->candev = netdev->link_data;

    fs_fd = vfs_open(priv->candev->dev_info->dev_name, O_RDWR);
    if (fs_fd < 0)
    {
        return fs_fd;
    }

    priv->fs_fd = fs_fd;
    fs_getfilep(fs_fd, &priv->filep);

    spin_lock_irqsave(&ACTIVE_CAN_SOCKETS_LOCK, pl_flags);

    ret = bind_active_can_sockets(priv);

    spin_unlock_irqrestore(&ACTIVE_CAN_SOCKETS_LOCK, pl_flags);

    return ret;
}

static int can_poll(struct netdev_sock *nsock, struct kpollfd *kfds, bool setup)
{
    can_sock_t *priv = nsock->priv;
    int ret;

    if (priv->candev == NULL || priv->filep == NULL ||
        priv->candev->dev_info->can_func->can_poll == NULL)
    {
        return -ENODEV;
    }

    ret = priv->candev->dev_info->can_func->can_poll(priv->filep, kfds, setup);

    return ret;
}

static ssize_t can_recvmsg(struct netdev_sock *nsock, struct msghdr *msg, int flags)
{
    NET_DEV *netdev = NULL;
    void *buf = msg->msg_iov->iov_base;
    size_t len = msg->msg_iov->iov_len;
    can_sock_t *priv = nsock->priv;
    int ret;

    if (priv->candev == NULL || priv->filep == NULL ||
        priv->candev->dev_info->can_func->can_recv == NULL)
    {
        return -EADDRNOTAVAIL;
    }

    netdev = priv->candev->netdev;

    ret = priv->candev->dev_info->can_func->can_recv(priv->filep, buf, len);
    if (ret > 0)
    {
        /* 无论是否是FD帧都当做标准帧去解析 */
        atomic64_inc(&netdev->ll_info.can.can_stats.in_frames);
        atomic64_add(&netdev->ll_info.can.can_stats.in_bytes, ((CAN_MSG *)buf)->can_dlc);
    }

    return ret;
}

static ssize_t can_sendmsg(struct netdev_sock *nsock, struct msghdr *msg, int flags)
{
    NET_DEV *netdev = NULL;
    can_sock_t *priv = nsock->priv;
    void *buf = msg->msg_iov->iov_base;
    size_t len = msg->msg_iov->iov_len;
    long send_flag;
    int ret;

    if (priv->candev == NULL || priv->filep == NULL ||
        priv->candev->dev_info->can_func->can_send == NULL)
    {
        return -ENODEV;
    }

    netdev = priv->candev->netdev;

    ret = priv->candev->dev_info->can_func->can_send(priv->filep, buf, len);

    if (ret > 0)
    {
        /* 无论是否是FD帧都当做标准帧去解析 */
        atomic64_inc(&netdev->ll_info.can.can_stats.out_frames);
        atomic64_add(&netdev->ll_info.can.can_stats.out_bytes, ((CAN_MSG *)buf)->can_dlc);
    }
    else
    {
        atomic64_inc(&netdev->ll_info.can.can_stats.eout_frames);
        atomic64_add(&netdev->ll_info.can.can_stats.eout_bytes, ((CAN_MSG *)buf)->can_dlc);
    }

    return ret;
}

static int can_close(struct netdev_sock *nsock)
{
    can_sock_t *priv = nsock->priv;
    irq_flags_t pl_flags;

    if (priv->candev == NULL || priv->filep == NULL)
    {
        return -ENODEV;
    }

    spin_lock_irqsave(&ACTIVE_CAN_SOCKETS_LOCK, pl_flags);
    unbind_active_can_sockets(priv);
    spin_unlock_irqrestore(&ACTIVE_CAN_SOCKETS_LOCK, pl_flags);

    printk("can_close\n");

    priv->candev = NULL;
    priv->fs_fd = -1;
    priv->filep = NULL;

    free(priv);

    return OK;
}

static int can_ioctl(struct netdev_sock *nsock, int cmd, unsigned long arg)
{
    struct ifreq *ifr;
    can_sock_t *priv = nsock->priv;
    NET_DEV *ndev;
    int arg_val;
    int ret = OK;

    if (priv->candev == NULL || priv->filep == NULL)
    {
        return -ENODEV;
    }

    switch (cmd)
    {
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

    default:
        if (priv->candev->dev_info->can_func->can_ioctl == NULL)
        {
            return -ENOTSUP;
        }
        ret = priv->candev->dev_info->can_func->can_ioctl(priv->filep, cmd, arg);
        break;
    }

    return ret;
}

static int can_getsockopt(struct netdev_sock *nsock, int level, int option, void *value,
                          socklen_t *value_len)
{
    return 0;
}

static int can_setsockopt(struct netdev_sock *nsock, int level, int option, const void *value,
                          socklen_t value_len)
{
    can_sock_t *priv = nsock->priv;
    struct timeval *timeout;

    if (level != SOL_CAN_RAW)
    {
        return -ENOTSUP;
    }

    switch (option)
    {
    case CAN_RAW_FILTER:
        priv->sock_opts |= BIT(*((int *)value));
        break;

    case CAN_RAW_ERR_FILTER:
        priv->sock_opts |= BIT(*((int *)value));
        break;

    case CAN_RAW_LOOPBACK:
        priv->sock_opts |= BIT(*((int *)value));
        break;

    case CAN_RAW_RECV_OWN_MSGS:
        priv->sock_opts |= BIT(*((int *)value));
        break;

    case CAN_RAW_FD_FRAMES:
        priv->sock_opts |= BIT(*((int *)value));
        break;

    case CAN_RAW_JOIN_FILTERS:
        priv->sock_opts |= BIT(*((int *)value));
        break;

    case CAN_RAW_TX_DEADLINE:
        priv->sock_opts |= BIT(*((int *)value));
        break;

    default:
        return -ENOPROTOOPT;
    }

    return OK;
}

static const struct sa_type can_supp_sa_types[] = {
    {.family = AF_CAN, .type = SOCK_RAW, .protocol = CAN_RAW},
    {.family = AF_CAN, .type = SOCK_RAW, .protocol = CAN_BCM},
    {.family = AF_CAN, .type = SOCK_RAW, .protocol = CAN_TP16},
    {.family = AF_CAN, .type = SOCK_RAW, .protocol = CAN_TP20},
    {.family = AF_CAN, .type = SOCK_RAW, .protocol = CAN_MCNET},
    {.family = AF_CAN, .type = SOCK_RAW, .protocol = CAN_ISOTP},
    {.family = AF_CAN, .type = SOCK_RAW, .protocol = CAN_J1939}};

static struct netdev_sock_ops can_netdev_ops = {
    .ns_setup = can_setup,
    .ns_sockcaps = NULL,
    .ns_addref = NULL,
    .ns_bind = can_bind,
    .ns_getsockname = NULL,
    .ns_getpeername = NULL,
    .ns_listen = NULL,
    .ns_connect = NULL,
    .ns_accept = NULL,
    .ns_poll = can_poll,
    .ns_sendmsg = can_sendmsg,
    .ns_recvmsg = can_recvmsg,
    .ns_close = can_close,
    .ns_ioctl = can_ioctl,
    .ns_socketpair = NULL,
    .ns_shutdown = NULL,
    .ns_getsockopt = can_getsockopt,
    .ns_setsockopt = can_setsockopt,
    .ns_sendfile = NULL,
};

static int netdev_register_can_sockif()
{
    if (netdev_register_net_stack_sockif(can_supp_sa_types, array_size(can_supp_sa_types),
                                         &can_netdev_ops) != 0)
    {
        KLOG_E("Register AF_CAN Socket Interfaces Failed");
    }
    else
    {
        KLOG_I("Register AF_CAN Socket Interfaces");
    }

    return OK;
}
INIT_EXPORT_SUBSYS(netdev_register_can_sockif, "Init AF_CAN Socket Interface");
