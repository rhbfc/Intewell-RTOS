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

#ifndef __NETDEV_H__
#define __NETDEV_H__

#include <list.h>
#include <atomic.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <net/net.h>

#ifdef __cplusplus
extern "C"
{
#endif

#include <system/macros.h>

#define NETDEV_NAME_MAX 16
#define OK 0
#define ERROR 1
#define ASSERT(x)
#define NETDEV_USING_IFCONFIG

#ifndef NETDEV_HWADDR_MAX_LEN
#define NETDEV_HWADDR_MAX_LEN 8U
#endif

#if NETDEV_IPV6
#ifndef NETDEV_IPV6_NUM_ADDRESSES
#define NETDEV_IPV6_NUM_ADDRESSES 3U
#endif
#endif /* NETDEV_IPV6 */

#define NETDEV_FLAG_STATUS 0x01U
#define NETDEV_FLAG_BROADCAST 0x02U
#define NETDEV_FLAG_LINK_STATUS 0x04U
#define NETDEV_FLAG_ETHARP 0x08U
#define NETDEV_FLAG_ETHERNET 0x10U
#define NETDEV_FLAG_IGMP 0x20U
#define NETDEV_FLAG_MLD6 0x40U
#define NETDEV_FLAG_INTERNET 0x80U
#define NETDEV_FLAG_DHCP 0x100U

#define NETDEV_FLAG_LOOKBACK 0x200U

#define NETDEV_FLAG_SET   1
#define NETDEV_FLAG_UNSET 0

#define IFF_IS_UP(f) (((f)&NETDEV_FLAG_STATUS) != 0)

#define NETDEV_TX_CONTINUE 1

#define IPBUF(hl) ((void *)(IOB_DATA (dev->d_iob) + (hl)))
#define NETLLBUF ((void *)((uint8_t *)IPBUF (0) - NET_LL_HDRLEN (dev)))

#define NETDEV_TO_ETHDEV(netdev) ((ETH_DEV *)netdev->link_data)

#define NETDEV_PKTSIZE(dev) (dev)->pkt_size

/* 获取AF_PACKET套接字收发统计数据 */
#define SIOCGAFPKTSTATS 0x89F0

enum netdev_cb_type
{
    NETDEV_CB_ADDR_IP,
    NETDEV_CB_ADDR_NETMASK,
    NETDEV_CB_ADDR_GATEWAY,
    NETDEV_CB_ADDR_DNS_SERVER,
    NETDEV_CB_STATUS_UP,
    NETDEV_CB_STATUS_DOWN,
    NETDEV_CB_STATUS_LINK_UP,
    NETDEV_CB_STATUS_LINK_DOWN,
    NETDEV_CB_STATUS_INTERNET_UP,
    NETDEV_CB_STATUS_INTERNET_DOWN,
    NETDEV_CB_STATUS_DHCP_ENABLE,
    NETDEV_CB_STATUS_DHCP_DISABLE,
    NETDEV_CB_REGISTER,
    NETDEV_CB_DEFAULT_CHANGE,
};

struct network_dev;
typedef struct network_dev NET_DEV;

typedef void (*netdev_callback_fn) (NET_DEV *netdev, enum netdev_cb_type type);

struct netdev_ops;
struct netdev_sock_ops;

struct netdev_sock_if
{
    struct list_node list;
    const struct sock_intf_s *psock_sockif; /* NETDEV组件向系统psocket注册支持的协议族类型与套接字相关接口 */
    const struct netdev_sock_ops *ops; /* 网络协议栈向NETDEV组件提供的套接字相关接口 */
};

struct netdev_sock
{
    struct sa_type satype;
    struct netdev_sock_if *sockif;
    void *priv; /* LwIP中分配的套接字号 */
};

struct netdev_sock_ops
{
    int (*ns_setup) (struct netdev_sock *nsock);
    sockcaps_t (*ns_sockcaps) (struct netdev_sock *nsock);
    void (*ns_addref) (struct netdev_sock *nsock);
    int (*ns_bind) (struct netdev_sock *nsock, const struct sockaddr *addr, socklen_t addrlen);
    int (*ns_getsockname) (struct netdev_sock *nsock, struct sockaddr *addr, socklen_t *addrlen);
    int (*ns_getpeername) (struct netdev_sock *nsock, struct sockaddr *addr, socklen_t *addrlen);
    int (*ns_listen) (struct netdev_sock *nsock, int backlog);
    int (*ns_connect) (struct netdev_sock *nsock, const struct sockaddr *addr, socklen_t addrlen);
    int (*ns_accept) (struct netdev_sock *nsock, struct sockaddr *addr, socklen_t *addrlen, struct netdev_sock *newsock, int flags);
    int (*ns_poll) (struct netdev_sock *nsock, struct kpollfd *fds, bool setup);
    ssize_t (*ns_sendmsg) (struct netdev_sock *nsock, struct msghdr *msg, int flags);
    ssize_t (*ns_recvmsg) (struct netdev_sock *nsock, struct msghdr *msg, int flags);
    int (*ns_close) (struct netdev_sock *nsock);
    int (*ns_ioctl) (struct netdev_sock *nsock, int cmd, unsigned long arg);
    int (*ns_socketpair) (struct netdev_sock *nsocks[2]);
    int (*ns_shutdown) (struct netdev_sock *nsock, int how);
    int (*ns_getsockopt) (struct netdev_sock *nsock, int level, int option, void *value, socklen_t *value_len);
    int (*ns_setsockopt) (struct netdev_sock *nsock, int level, int option, const void *value, socklen_t value_len);
    ssize_t (*ns_sendfile) (struct netdev_sock *nsock, struct file *infile, off_t *offset, size_t count);
};

extern int netdev_register_net_stack_sockif (const struct sa_type *types, const size_t types_count, const struct netdev_sock_ops *ops);

struct PKT_STATS
{
    uint64_t out_bytes; /* 发送字节数 */
    uint64_t out_pacs;  /* 发送包数 */
    uint64_t eout_pacs; /* 发送失败包数 */
    uint64_t dout_pacs; /* 发送丢弃包数 */
    uint64_t in_bytes;  /* 接收字节数 */
    uint64_t in_pacs;   /* 接收包数 */
    uint64_t ein_pacs;  /* 接收失败包数 */
    uint64_t din_pacs;  /* 接收丢弃包数 */
};

typedef struct afpacket_stats
{
    atomic64_t out_bytes;  /* 发送字节数 */
    atomic64_t out_pacs;   /* 发送包数 */
    atomic64_t eout_bytes; /* 发送失败字节数 */
    atomic64_t eout_pacs;  /* 发送失败包数 */
    atomic64_t in_bytes;   /* 接收字节数 */
    atomic64_t in_pacs;    /* 接收包数 */
    atomic64_t din_bytes;  /* 接收丢弃字节数 */
    atomic64_t din_pacs;   /* 接收丢弃包数 */
} AFPACKET_STATS;

typedef struct afcan_stats
{
    atomic64_t out_frames;    /* 发送帧数 */
    atomic64_t out_bytes;     /* 发送字节数 */
    atomic64_t eout_frames;   /* 发送失败帧数 */
    atomic64_t eout_bytes;    /* 发送失败字节数 */
    atomic64_t in_frames;     /* 接收帧数 */
    atomic64_t in_bytes;      /* 接收字节数 */
    atomic64_t din_frames;    /* 接收丢弃帧数 */
    atomic64_t din_bytes;     /* 接收丢弃字节数 */
} AFCAN_STATS;

typedef struct network_dev
{
    struct list_node node;

    char name[NETDEV_NAME_MAX];
    int  netdev_unit;

    union
    {
        struct
        {
            struct in_addr ip_addr;
            struct in_addr netmask;
            struct in_addr gw;
            uint16_t mtu;
            struct PKT_STATS pkt_stats;
            AFPACKET_STATS afpkt_stats;
        } ethernet;
        struct
        {
            AFCAN_STATS can_stats;
        } can;
    } ll_info;

    uint8_t hwaddr_len;
    uint8_t hwaddr[NETDEV_HWADDR_MAX_LEN];

    uint8_t ll_type;
    uint8_t ll_hdrlen;
    uint32_t flags;    /* NetDev设备相关标志位 */

    const struct netdev_ops *ops;

    netdev_callback_fn status_callback; /* NetDev网卡状态改变回调 */
    netdev_callback_fn addr_callback;   /* NetDev网卡地址改变回调 */

    void *link_data; /* 不应保存协议栈的结构体，而是应该保存链路层结构体指针，如ETH_DEV * */
}NET_DEV;

/* NetDev设备链表 */
extern NET_DEV *NETDEV_LIST;

typedef struct ttos_spinlock ttos_spinlock_t;
extern ttos_spinlock_t netdev_spinlock;

extern NET_DEV *netdev_lo;

struct netdev_ping_resp
{
    struct in_addr ip_addr;
    uint16_t data_len;
    uint16_t ttl;
    uint32_t ticks;
    void *user_data;
};

struct netdev_ops
{
    int (*set_up) (NET_DEV *netdev);
    int (*set_down) (NET_DEV *netdev);

    int (*set_link_up) (NET_DEV *netdev);
    int (*set_link_down) (NET_DEV *netdev);

    int (*set_flag) (NET_DEV *netdev, uint32_t flag, uint32_t newstat);

    int (*set_mac) (NET_DEV *netdev, unsigned long arg);
    int (*set_addr_info) (NET_DEV *netdev, struct in_addr *ip_addr, struct in_addr *netmask, struct in_addr *gw);
    int (*set_mtu) (NET_DEV *netdev, int mtu);
    int (*set_dns_server) (NET_DEV *netdev, uint8_t dns_num, struct in_addr *dns_server);
    int (*set_dhcp) (NET_DEV *netdev, unsigned int is_enable);

#ifdef CONFIG_SHELL
    int (*ping) (NET_DEV *netdev, const char *host, size_t data_len, uint32_t timeout, struct netdev_ping_resp *ping_resp, int isbind);
    void (*netstat) (NET_DEV *netdev);
#endif

    int (*set_default) (NET_DEV *netdev);
};

typedef int (*netdev_install_cb)(NET_DEV *, void *);

NET_DEV *netdev_get_first_by_flags (uint16_t flags);
NET_DEV *netdev_get_by_ipaddr (struct in_addr *ip_addr);
NET_DEV *netdev_get_by_name (const char *name);
NET_DEV *netdev_get_by_unit (const int unit);
NET_DEV *netdev_get_default(void);

short int netdev_flags_to_posix_flags (NET_DEV *ndev);

void netdev_set_default (NET_DEV *netdev);
void netdev_set_default_change_callback (netdev_callback_fn register_callback);

int netdev_set_up (NET_DEV *netdev);
int netdev_set_down (NET_DEV *netdev);
int netdev_set_link_up (NET_DEV *netdev);
int netdev_set_link_down (NET_DEV *netdev);
int netdev_set_dhcp (NET_DEV *netdev, int is_enable);

#define netdev_is_up(netdev) (((netdev)->flags & NETDEV_FLAG_STATUS) ? (uint8_t)1 : (uint8_t)0)
#define netdev_is_link_up(netdev)  (((netdev)->flags & NETDEV_FLAG_LINK_STATUS) ? (uint8_t)1 : (uint8_t)0)
#define netdev_is_internet_up(netdev) (((netdev)->flags & NETDEV_FLAG_INTERNET) ? (uint8_t)1 : (uint8_t)0)
#define netdev_is_dhcp_enabled(netdev) (((netdev)->flags & NETDEV_FLAG_DHCP) ? (uint8_t)1 : (uint8_t)0)

int netdev_set_mac (NET_DEV *netdev, unsigned long arg);
int netdev_set_ipaddr (NET_DEV *netdev, const struct in_addr *ipaddr);
int netdev_set_netmask (NET_DEV *netdev, const struct in_addr *netmask);
int netdev_set_gw (NET_DEV *netdev, const struct in_addr *gw);

void netdev_set_register_callback (netdev_callback_fn status_callback);
void netdev_set_status_callback (NET_DEV *netdev, netdev_callback_fn status_callback);
void netdev_set_addr_callback (NET_DEV *netdev, netdev_callback_fn addr_callback);

void netdev_sync_ipaddr (NET_DEV *netdev, const struct in_addr *ipaddr);
void netdev_sync_netmask (NET_DEV *netdev, const struct in_addr *netmask);
void netdev_sync_gw (NET_DEV *netdev, const struct in_addr *gw);
void netdev_sync_dhcp_status (NET_DEV *netdev, int is_enable);

NET_DEV *netdev_install_dev (void *link_data, enum net_lltype link_type, const struct netdev_ops *ops, netdev_install_cb callback, void *cb_data);
int netdev_uninstall_dev (NET_DEV *dev);
NET_DEV *netdev_install_loopback (void *lookback_dev, netdev_install_cb callback, void *netstack_data);

static inline int is_zero_ethaddr(const uint8_t *addr)
{
    return !(addr[0] | addr[1] | addr[2] | addr[3] | addr[4] | addr[5]);
}

static inline int is_multicast_ethaddr(const uint8_t *addr)
{
    return 0x01 & addr[0];
}

static inline int is_broadcast_ethaddr(const uint8_t *addr)
{
    return (addr[0] & addr[1] & addr[2] & addr[3] & addr[4] & addr[5]) ==
        0xff;
}

static inline int is_valid_ethaddr(const uint8_t *addr)
{
    return !is_multicast_ethaddr(addr) && !is_zero_ethaddr(addr);
}

#ifdef __cplusplus
}
#endif

#endif /* __NETDEV_H__ */
