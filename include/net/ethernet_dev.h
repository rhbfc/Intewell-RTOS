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

#ifndef __NETIF_ETHERNETIF_H__
#define __NETIF_ETHERNETIF_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <system/kconfig.h>
#include <spinlock.h>
#include <wqueue.h>
#include <fs/fs.h>
#include <net/net.h>
#include <netinet/in.h>
#include <list.h>
#include <net/netdev.h>
#include <driver/device.h>
#include <linux/wait.h>

#define ETH_HDRLEN           14            /* 2 * 6 + 2 */
#define ETHERNET_DEFAULT_MTU 1500
#define ETHERNET_MIN_MTU     46
#define ETH_FCSLEN           4
#define ETH_FRAME_MAX_LEN    (ETH_HDRLEN + ETHERNET_DEFAULT_MTU + ETH_FCSLEN)
#define ETH_FRAME_MIN_LEN    (ETH_HDRLEN + ETHERNET_MIN_MTU)

#define TPID_8021QVLAN   ETHERTYPE_VLAN

#define DRV_NAME_MAX       16
#define ETH_HW_ADDR_LEN    6
#define ETH_NAME_MAX       8
#define ETH_IP_MAX         16
#define ETH_MASK_MAX       16
#define ETH_GATEWAY_MAX    16

#if LWIP_NETIF_HOSTNAME
#define LWIP_HOSTNAME_LEN 16
#else
#define LWIP_HOSTNAME_LEN 0
#endif

#define NETPKT_WITH_NETSTACK    ((void *)0)
#if IS_ENABLED(CONFIG_OS_LP64)
#define NETPKT_STANDALONE  ((void *)(0xFFFFFFFFFFFFFFFFULL))
#else
#define NETPKT_STANDALONE  ((void *)(0xFFFFFFFFU))
#endif

#define ETH_NETPKT_STANDALONE            (1U << 0)   /* 独立的netpkt结构,即netpkt为独立分配,未包含在其他结构体中 */
#define ETH_NETPKT_DATA_SEPARATED        (1U << 1)   /* netpkt中不包括实际数据负载,成员buf指向其他地址空间或为空 */
#define ETH_NETPKT_DATA_FREE_INITIATIVE  (1U << 2)   /* netpkt中不包括实际数据负载,释放netpkt时需要主动释放buf指向的地址空间 */
#define ETH_NETPKT_BUF_TO_PBUF           (1U << 3)   /* netpkt的成员buf指向一个pbuf结构 */
#define ETH_NETPKT_WITHIN_PBUF           (1U << 4)   /* netpkt结构存在于pbuf的payload空间中 */

#define LOOPBACK_UNIT -1
#define IS_LOOPBACK_DEV(ethdev) ((ethdev->eth_unit == LOOPBACK_UNIT) ? 1 : 0)

#define PCAP_OP_START      1 /* Start a capture session */
#define PCAP_OP_STOP       2 /* Stop a capture session */
#define PCAP_FILENAME_SIZE 256

#define ETH_PARAM_END_OF_LIST   0x0000
#define ETH_PARAM_INT32         0x0001
#define ETH_PARAM_INT64         0x0002
#define ETH_PARAM_STRING        0x0003
#define ETH_PARAM_POINTER       0x0004

#define ETH_MDIO_READ(ethdev)    ethdev->drv_info->mdio_func->mdio_read
#define ETH_MDIO_WRITE(ethdev)   ethdev->drv_info->mdio_func->mdio_write
#define ETH_MDIO45_READ(ethdev)  ethdev->drv_info->mdio_func->mdio45_read
#define ETH_MDIO45_WRITE(ethdev) ethdev->drv_info->mdio_func->mdio45_write

typedef struct ethernet_dev ETH_DEV;
typedef struct eth_netpkt ETH_NETPKT;
typedef struct phy_dev PHY_DEV;
typedef struct media_list MEDIA_LIST;
typedef int (*ETH_ITERATE_HANDLER)(ETH_DEV *ethdev, void *arg);


typedef int (*MDIO_READ_FUNC) (void *dev, unsigned char phy_addr, unsigned char reg_addr, unsigned short *data_val);
typedef int (*MDIO_WRITE_FUNC) (void *dev, unsigned char phy_addr, unsigned char reg_addr, unsigned short data_val);
typedef int (*MDIO45_READ_FUNC) (void *dev, unsigned char phy_addr, unsigned char dev_addr, unsigned char reg_addr, unsigned short *data_val);
typedef int (*MDIO45_WRITE_FUNC) (void *dev, unsigned char phy_addr, unsigned char dev_addr, unsigned char reg_addr, unsigned short data_val);
typedef int (*CUSTOM_INIT_FUNC) (PHY_DEV *phydev);

typedef enum
{
    MAC = 0,
    MDIO
} MDIO_PARENT_TYPE;

/* 由MAC或MDIO驱动实现 */
typedef struct
{
    MDIO_READ_FUNC mdio_read;
    MDIO_WRITE_FUNC mdio_write;
    MDIO45_READ_FUNC mdio45_read;
    MDIO45_WRITE_FUNC mdio45_write;
    unsigned int preasgn_phyaddr;    /* MAC预设PHY地址，无需扫描 */
    unsigned int skip_gen_init;      /* 不使用通用PHY驱动的gen_init()函数 */
    CUSTOM_INIT_FUNC custom_init;    /* 使用通用PHY驱动但需自定义PHY硬件初始化函数可使用该接口指针 */
} MDIO_FUNCS;

typedef enum
{
    clause22 = 0,
    clause45
} MDIO_TYPE;

typedef struct
{
    struct list_node node;
    ETH_DEV *ethdev;
    unsigned int phy_handle;
} WAIT_NODE;

typedef struct
{
    long len;
    char *table;
} MULTI_TABLE;

typedef enum
{
    fdt = 0,
    pcie,
    mmio
} eth_bus_type;

typedef enum
{
    with_netstack = 0,
    standalone
} ETH_NETPKT_TYPE;

typedef struct {
    char dst[6];
    char src[6];
    uint16_t etype;
} ether_header_t;

typedef struct eth_netpkt
{
    void *base;             /* 指向网络协议栈数据包结构或netpkt自身，驱动开发者无需关心 */
    ETH_NETPKT *next;
    atomic_t refs;
    unsigned int flags;     /* netpkt类型标志位 */
    unsigned int len;       /* 数据长度 */
    unsigned char *buf;     /* 数据基址 */
    unsigned int reserved_len;
    unsigned char *reserved;
} __attribute__((aligned(CONFIG_NETPKT_ALIGNMENT_LEN))) ETH_NETPKT;

typedef struct eth_dev_funcs
{
    int (*ifup) (ETH_DEV *ethdev);
    int (*ifdown) (ETH_DEV *ethdev);
    int (*unload) (ETH_DEV *ethdev);
    int (*ioctl) (ETH_DEV *ethdev, int cmd, unsigned long arg);
    int (*send) (ETH_DEV *ethdev, ETH_NETPKT *netpkt);
    int (*receive) (ETH_DEV *ethdev);
    int (*mCastAddrAdd) (ETH_DEV *ethdev, char *addr);
    int (*mCastAddrDel) (ETH_DEV *ethdev, char *addr);
    int (*mCastAddrGet) (ETH_DEV *ethdev, MULTI_TABLE *table);
    int (*pollSend) (ETH_DEV *ethdev, ETH_NETPKT *netpkt);
    int (*pollRcv) (ETH_DEV *ethdev, ETH_NETPKT *netpkt);
    int (*mac_get) (ETH_DEV *ethdev, char *mac_addr);
    int (*mac_set) (ETH_DEV *ethdev, const char *mac_addr);
    int (*link_update) (ETH_DEV *ethdev);
} ETH_DEV_FUNCS;

typedef union eth_param_value
{
    void *point_val;
    unsigned int int32_val;
    unsigned long long int64_val;
    char *string_val;
} ETH_PARAM_VALUE;

typedef struct eth_drv_param
{
    char *param_name;
    unsigned int param_type;
    ETH_PARAM_VALUE value;
} ETH_DRV_PARAM;

typedef struct eth_drv_info
{
    struct list_node node;
    char *drv_name;           /* 驱动名称 */
    char *drv_desc;           /* 驱动描述 */
    eth_bus_type bus;         /* 设备总线类型 */
    ETH_DRV_PARAM *drv_param; /* 驱动参数 */
    int drv_flags;            /* 驱动标志位 */
    int ref_cnt;              /* 引用次数 */
    ETH_DEV_FUNCS *eth_func;  /* 功能表 */
    MDIO_FUNCS *mdio_func;    /* MDIO功能表 */
} ETH_DRV_INFO;

/* 注意： 本结构体变量必须在以太网设备驱动控制结构体的第一个 */
typedef struct ethernet_dev
{
    struct list_node node;
    NET_DEV *netdev;        /* netdev指针 */
    struct device *device_info;
    int eth_unit;           /* 以太网设备单元号 */
    int init_flag;

    ETH_DRV_INFO *drv_info; /* 设备驱动信息 */

    ttos_spinlock_t ip_lock; /* 在增删多IP时使用 */

#ifdef CONFIG_LWIP_2_2_0
    struct netif *lwip_netif[CONFIG_MAX_NETIF_IPS];
    int lwip_netif_count;
#endif

    /* 用于网卡数据接收任务 */
    SEMA_ID rx_sem;
    TASK_ID rx_task;

    ttos_spinlock_t send_lock;

    unsigned int phy_handle; /* 设备树phy-handle的值 */
    MDIO_TYPE mdio_type;
    PHY_DEV *phydev;
    unsigned int phy_status; /* PHY状态(IFM_ACTIVE IFM_AVALID) */
    unsigned int phy_media;  /* PHY链路模式(IFM_1000_T IFM_FDX等) */
    MEDIA_LIST *media_list;
    MUTEX_ID phy_mutex;

    wait_queue_head_t wq;
    atomic_t wait_rx;

    atomic_t refcnt;        /* 引用计数 */

    void *dev_ctrl; /* 网络设备驱动管理结构体 */
} ETH_DEV;

/* 以太网卡网络配置信息 */
typedef struct eth_cfg_info
{
    unsigned char mac_addr[ETH_HW_ADDR_LEN];
    struct in_addr ip;
    struct in_addr netmask;
    struct in_addr gateway;
    bool auto_start;
    bool is_default;
} ETH_CFG_INFO;

/* PCAP 所需结构 */
struct pcap_nsock
{
    struct list_node node;
    struct netdev_sock *nsock;
    MSGQ_ID msgq;
    ETH_DEV *eth;
};

typedef struct pcap_ioctl_struct
{
    int op;
    int type;
    union
    {
        char filename[PCAP_FILENAME_SIZE];
        struct sockaddr dst;
    } d;
} pcap_ioctl_t;


extern struct netdev_ops *eth_stack_netdev_ops;
extern struct list_head PCAP_NSOCK_LIST;
extern MUTEX_ID         PCAP_SOCKLIST_MUTEX;
extern ttos_spinlock_t  ETH_GLOBAL_LOCK;

int eth_device_init (ETH_DRV_INFO *drv_info, void *dev_ctrl, struct device *device);
int eth_device_deinit (ETH_DEV *ethdev);
int eth_sync_netdev_flags (ETH_DEV *ethdev, void *net_stack);
void ETH_DATA_TO_STACK (ETH_DEV *ethdev, ETH_NETPKT *netpkt);
ETH_NETPKT *eth_netpkt_alloc (unsigned int len);
void eth_netpkt_inc_ref(ETH_NETPKT *netpkt);
int eth_netpkt_free (ETH_NETPKT *netpkt);
int eth_netpkt_to_desc(void *desc_buf, ETH_NETPKT *netpkt);
ETH_NETPKT *eth_netpkt_clone (const ETH_NETPKT *netpkt);
void eth_notify_rx_task (ETH_DEV *ethdev);
ETH_DRV_INFO *eth_drv_find_by_name (char *drv_name);
int eth_get_arp_entry (int i, char *ip, char *mac, unsigned char *state, char *dev_name);
void eth_arp_announce(ETH_DEV *ethdev);
uint16_t eth_network_protocol_parse (ether_header_t *hdr);
char eth_ip_protocol_parse (void *pkt);
unsigned char *eth_src_addr_parse (void *pkt);
short eth_src_port_parse (void *pkt);
bool eth_is_broadcast_addr (const unsigned char *addr);
int eth_update_resolv_file (int dns_count, const char *dns_server);
ETH_DEV *eth_find_wait_list (unsigned int phandle);
ETH_DEV *eth_find_by_name(char *eth_name);
int eth_device_iterate(ETH_ITERATE_HANDLER handler, void *arg);
void eth_device_get(ETH_DEV *ethdev);
void eth_device_put(ETH_DEV *ethdev);

#ifdef __cplusplus
}
#endif

#endif /* __NETIF_ETHERNETIF_H__ */
