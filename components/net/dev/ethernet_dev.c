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

#include <driver/of.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <system/kconfig.h>
#include <stdlib.h>

#include <net/ethernet.h>
#include <net/ethernet_dev.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/netdev.h>
#include <net/packet.h>

#include <symtab.h>

#if IS_ENABLED(CONFIG_LWIP_2_2_0)
#include <lwip/tcpip.h>
#include <net/lwip_ethernet.h>
#include <netif/bridgeif.h>
#include <netif/etharp.h>
#if LWIP_IPV6
#include "lwip/ethip6.h"
#endif /* LWIP_IPV6 */
#endif

#undef KLOG_TAG
#define KLOG_TAG "ETHIF"
#include <klog.h>

#define RESOLV_CONF_FILE_PATH "/etc/resolv.conf"
#define LOOPBACK_UNIT -1

/* 系统内以太网设备总数 */
static int eth_dev_count;
static LIST_HEAD(ETH_DEV_LIST);
static LIST_HEAD(ETH_DRV_LIST);

struct netdev_ops *eth_stack_netdev_ops = NULL;
netdev_install_cb eth_netdev_install_cb = NULL;

/* 环回网卡 */
ETH_DEV ethdev_lo;

DEFINE_SPINLOCK(ETH_GLOBAL_LOCK);

/* pcap socket链表 */
LIST_HEAD(PCAP_NSOCK_LIST);
MUTEX_ID PCAP_SOCKLIST_MUTEX = NULL;

#if IS_ENABLED(CONFIG_LWIP_2_2_0)
extern TASK_ID tcpip_thread_taskid;
#endif
extern struct list_node ETH_WAIT_ATTACH_LIST;
extern int phy_device_init();
extern int phy_attach_orphan_dev(ETH_DEV *ethdev);

static inline void eth_wait_for_notify(ETH_DEV *ethdev)
{
    wait_event(&ethdev->wq, atomic_cas(&ethdev->wait_rx, 1, 0));
}

/* 根据驱动名称获取已注册的驱动信息结构体 */
ETH_DRV_INFO *eth_drv_find_by_name(char *drv_name)
{
    ETH_DRV_INFO *drv_info;
    long eth_flags;

    if (drv_name == NULL)
    {
        return NULL;
    }

    spin_lock_irqsave(&ETH_GLOBAL_LOCK, eth_flags);

    list_for_each_entry(drv_info, &ETH_DRV_LIST, node)
    {
        if (strcmp(drv_info->drv_name, drv_name) == 0)
        {
            spin_unlock_irqrestore(&ETH_GLOBAL_LOCK, eth_flags);
            return drv_info;
        }
    }

    spin_unlock_irqrestore(&ETH_GLOBAL_LOCK, eth_flags);

    return NULL;
}

/* 注册以太网设备驱动信息 */
void eth_drv_register(ETH_DRV_INFO *drv)
{
    ETH_DRV_INFO *drv_info;
    long eth_flags;

    if (drv == NULL)
    {
        return;
    }

    spin_lock_irqsave(&ETH_GLOBAL_LOCK, eth_flags);

    list_for_each_entry(drv_info, &ETH_DRV_LIST, node)
    {
        if (strcmp(drv_info->drv_name, drv->drv_name) == 0)
        {
            drv_info->ref_cnt += 1;

            spin_unlock_irqrestore(&ETH_GLOBAL_LOCK, eth_flags);

            return;
        }
    }

    drv->ref_cnt = 1;

    list_add_tail(&drv->node, &ETH_DRV_LIST);

    spin_unlock_irqrestore(&ETH_GLOBAL_LOCK, eth_flags);

    return;
}

/* 注销以太网设备驱动信息 */
/* TODO 目前没用到该接口，后续考虑ETHIF锁和IP锁的竞争 */
int eth_drv_unregister(ETH_DRV_INFO *drv)
{
    ETH_DRV_INFO *drv_info;
    long eth_flags;

    if (drv == NULL)
    {
        return -ENODEV;
    }

    spin_lock_irqsave(&ETH_GLOBAL_LOCK, eth_flags);

    list_for_each_entry(drv_info, &ETH_DRV_LIST, node)
    {
        if (strcmp(drv_info->drv_name, drv->drv_name) == 0)
        {
            if (drv_info->ref_cnt > 1)
            {
                drv_info->ref_cnt -= 1;

                spin_unlock_irqrestore(&ETH_GLOBAL_LOCK, eth_flags);

                return -EBUSY;
            }

            list_delete(&drv->node);

            spin_unlock_irqrestore(&ETH_GLOBAL_LOCK, eth_flags);

            return OK;
        }
    }

    spin_unlock_irqrestore(&ETH_GLOBAL_LOCK, eth_flags);

    return OK;
}

int eth_drv_param_get(ETH_DEV *ethdev, char *param_name, unsigned int param_type,
                      ETH_PARAM_VALUE *value)
{
    ETH_DRV_PARAM *drv_param;
    int ret = -1;

    if (ethdev == NULL)
    {
        return (-1);
    }

    if ((drv_param = ethdev->drv_info->drv_param) == NULL)
    {
        return (-1);
    }

    while (drv_param->param_name != NULL)
    {
        if ((drv_param->param_type == param_type) &&
            (strcmp(drv_param->param_name, param_name) == 0))
        {
            value->int64_val = drv_param->value.int64_val;
            ret = OK;

            break;
        }
        drv_param++;
    }

    return (ret);
}

/* 根据以太网设备名获取ETH_DEV结构体 */
ETH_DEV *eth_find_by_name(char *eth_name)
{
    ETH_DEV *ethdev = NULL;
    long eth_flags;

    if (eth_name == NULL)
    {
        return NULL;
    }

    spin_lock_irqsave(&ETH_GLOBAL_LOCK, eth_flags);

    list_for_each_entry(ethdev, &ETH_DEV_LIST, node)
    {
        if (strcmp(ethdev->netdev->name, eth_name) == 0)
        {
            spin_unlock_irqrestore(&ETH_GLOBAL_LOCK, eth_flags);
            return ethdev;
        }
    }

    spin_unlock_irqrestore(&ETH_GLOBAL_LOCK, eth_flags);

    return NULL;
}

/* 增加以太网设备引用计数 */
KSYM_EXPORT(eth_device_get);
void eth_device_get(ETH_DEV *ethdev)
{
    if (ethdev == NULL || ethdev->init_flag != 0xfeedcafe)
    {
        return;
    }

    atomic_inc_return(&ethdev->refcnt);
}

/* 减少以太网设备引用计数 */
KSYM_EXPORT(eth_device_put);
void eth_device_put(ETH_DEV *ethdev)
{
    if (ethdev == NULL)
    {
        return;
    }

    atomic_dec_return(&ethdev->refcnt);
}

int eth_device_iterate(ETH_ITERATE_HANDLER handler, void *arg)
{
    ETH_DEV *ethdev = NULL;
    ETH_DEV **ethdev_snap = NULL;
    long eth_flags;
    int count = 0;
    int i;

    if (handler == NULL)
    {
        return -EINVAL;
    }

    spin_lock_irqsave(&ETH_GLOBAL_LOCK, eth_flags);

    list_for_each_entry(ethdev, &ETH_DEV_LIST, node)
    {
        count++;
    }

    if (count > 0)
    {
        ethdev_snap = malloc(sizeof(ETH_DEV *) * count);
        if (ethdev_snap != NULL)
        {
            i = 0;
            list_for_each_entry(ethdev, &ETH_DEV_LIST, node)
            {
                /* 增加引用计数，防止设备在遍历过程中被释放 */
                eth_device_get(ethdev);
                ethdev_snap[i++] = ethdev;
            }
        }
    }

    spin_unlock_irqrestore(&ETH_GLOBAL_LOCK, eth_flags);

    if (ethdev_snap != NULL)
    {
        for (i = 0; i < count; i++)
        {
            handler(ethdev_snap[i], arg);
            eth_device_put(ethdev_snap[i]);
        }
        free(ethdev_snap);
    }
    else
    {
        return -ENOMEM;
    }

    return OK;
}

int eth_get_arp_entry(int i, char *ip, char *mac, unsigned char *state, char *dev_name)
{
    int ret;

#if CONFIG_LWIP_2_2_0
    ret = eth_lwip_get_arp_entry(i, ip, mac, state, dev_name);
#else
    ret = -ENOTSUP;
#endif

    return ret;
}

void eth_arp_announce(ETH_DEV *ethdev)
{
#ifdef CONFIG_LWIP_2_2_0
    eth_lwip_arp_broadcast(ethdev);
#endif
}

/* 以太网报文网络层类型解析 */
uint16_t eth_network_protocol_parse(ether_header_t *hdr)
{
    unsigned char *eth_payload = NULL;
    unsigned short *pptp_type;

    if (hdr == NULL)
    {
        return (uint16_t)(-1);
    }

    return __ntohs(hdr->etype);
}

/* 网络传输层协议类型解析 */
inline char eth_ip_protocol_parse(void *pkt)
{
    unsigned char *eth_payload = NULL;
    unsigned short pptp_type;

    if (pkt == NULL)
    {
        return (-1);
    }

    eth_payload = (unsigned char *)pkt;

    pptp_type = eth_network_protocol_parse(pkt);
    if (pptp_type != ETHERTYPE_IP)
    {
        return (-1);
    }

    switch (*(eth_payload + 23))
    {
    case IPPROTO_ICMP:
    case IPPROTO_IGMP:
    case IPPROTO_UDP:
    case IPPROTO_UDPLITE:
    case IPPROTO_TCP:
        return (*(eth_payload + 23));
    default:
        return (-1);
    }
}

/* 返回以太网报文源IP地址 */
inline unsigned char *eth_src_addr_parse(void *pkt)
{
    unsigned char *eth_payload = NULL;
    unsigned short pptp_type;

    if (pkt == NULL)
    {
        return (NULL);
    }

    eth_payload = (unsigned char *)pkt;

    pptp_type = eth_network_protocol_parse(pkt);
    if (pptp_type == ETHERTYPE_IP)
    {
        return (eth_payload + 26);
    }
    else if (pptp_type == ETHERTYPE_ARP)
    {
        return (eth_payload + 38);
    }
    else
    {
        return (NULL);
    }
}

/* 传输层端口号解析 */
inline short eth_src_port_parse(void *pkt)
{
    unsigned char *eth_payload = NULL;
    unsigned char pptp_type;
    unsigned short *port;

    if (pkt == NULL)
    {
        return (-1);
    }

    eth_payload = (unsigned char *)pkt;

    pptp_type = eth_ip_protocol_parse(pkt);
    if (pptp_type == IPPROTO_TCP || pptp_type == IPPROTO_UDP)
    {
        port = (unsigned short *)(eth_payload + 34);
        return ((short)__htons(*port));
    }

    return (-1);
}

inline bool eth_is_broadcast_addr(const unsigned char *addr)
{
    return (*(const unsigned char *)(addr + 0) & *(const unsigned char *)(addr + 1) &
            *(const unsigned char *)(addr + 2) & *(const unsigned char *)(addr + 3) &
            *(const unsigned char *)(addr + 4) & *(const unsigned char *)(addr + 5) == 0xFF);
}

/* 以太网设备数据接收任务 */
static void eth_rx_task(ETH_DEV *ethdev)
{
    int len;
    T_TTOS_ReturnCode ret;

#ifdef CONFIG_ETH_RX_TASK_BIND_CPU_CORE
#ifdef CONFIG_SMP
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(CONFIG_ETH_RX_TASK_AFFINITY, &cpuset);
    (void)TTOS_SetTaskAffinity(((TASK_ID)TTOS_SELF_OBJECT_ID), &cpuset);
#endif
#endif

    while (1)
    {
        /* 等待网卡驱动通知有数据到达 */
        eth_wait_for_notify(ethdev);

        if (!netdev_is_up(ethdev->netdev))
        {
            /* Interface down, drop frame */
            ethdev->netdev->ll_info.ethernet.pkt_stats.din_pacs += 1;
            continue;
        }

        while (1)
        {
            /* 调用驱动数据接收函数 */
            len = ethdev->drv_info->eth_func->receive(ethdev);
            if (len == 0)
            {
                break;
            }
        }
    }
}

/*
  开启DHCP时，协议栈应调用本函数更新/etc/resolv.conf文件
  形参dns_server应该是由“;"隔离的DNS服务器IP地址，如"8.8.8.8;114.114.114.114"
*/
int eth_update_resolv_file(int dns_count, const char *dns_server)
{
    ETH_DEV *ethdev;
    int update_flag = 0;
    struct file f;
    int ret;
    int i;
    char buf[512];
    int offset = 0;
    char *p1 = (char *)dns_server;
    char *p2 = NULL;
    unsigned long len = 0;
    long eth_flags;

    spin_lock_irqsave(&ETH_GLOBAL_LOCK, eth_flags);

    /* 遍历以太网设备，仅当有设备是开启DHCP时才更新/etc/resolv.conf文件 */
    list_for_each_entry(ethdev, &ETH_DEV_LIST, node)
    {
        if (netdev_is_dhcp_enabled(ethdev->netdev))
        {
            update_flag = 1;
            break;
        }
    }

    spin_unlock_irqrestore(&ETH_GLOBAL_LOCK, eth_flags);

    if (update_flag)
    {
        ret = file_open(&f, RESOLV_CONF_FILE_PATH, O_WRONLY | O_CREAT | O_TRUNC,
                        S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (ret < 0)
        {
            KLOG_E("Failed to open resolv.conf!");

            return -ERROR;
        }

        memset(buf, 0, sizeof(buf));

        sprintf(buf, "%s",
                "##################################################################################"
                "############\n"
                "# When you enable DHCP, this file is automatically generated by the OS, Please "
                "DON'T modify! #\n"
                "##################################################################################"
                "############\n\n\n");

        offset = strlen(buf);

        for (i = 0; i < dns_count; i++)
        {
            p2 = strchr((const char *)p1, ';');
            if (p2 == NULL)
            {
                break;
            }

            len = (unsigned long)p2 - (unsigned long)p1;
            snprintf(buf + offset, strlen("nameserver ") + len + 1, "nameserver %s", p1);
            strcat(buf, "\n");
            offset = strlen(buf);

            p1 = p2 + 1;
        }

        file_write(&f, buf, strlen(buf));

        file_close(&f);
    }

    return 0;
}

void eth_notify_rx_task(ETH_DEV *ethdev)
{
    if (NULL == ethdev)
    {
        return;
    }

    atomic_set(&ethdev->wait_rx, 1);
    wake_up(&ethdev->wq);
}

/* 以太网设备注册 */
KSYM_EXPORT(eth_device_init);
int eth_device_init(ETH_DRV_INFO *drv_info, void *dev_ctrl, struct device *device)
{
    ETH_DEV *ethdev = (ETH_DEV *)dev_ctrl;
    NET_DEV *netdev;
    WAIT_NODE *wait_node;
    void *netstack_data = NULL;
    int ret;
    char rx_task_name[32];
    unsigned int value;
    char mac_addr[IFHWADDRLEN];
    ETH_PARAM_VALUE param_value;
    long eth_flags;

    if ((drv_info == NULL) || (dev_ctrl == NULL) || (device == NULL))
    {
        return -ERROR;
    }

    if (ethdev->init_flag == 0xfeedcafe)
    {
        KLOG_E("Ethernet Device Already Initialized!");
        return -ERROR;
    }

    ethdev->eth_unit = eth_dev_count++;
    ethdev->dev_ctrl = dev_ctrl;
    ethdev->drv_info = drv_info;
    ethdev->device_info = device;

    if (ethdev->drv_info->eth_func->mac_get == NULL)
    {
        KLOG_E("Ethernet Device Driver Should Supply MAC Address Acquire Function!");
        KLOG_E("Now Use The Randrom Address.");
        mac_addr[0] = 0xca;
        mac_addr[1] = 0xfe;
        mac_addr[2] = (char)rand();
        mac_addr[3] = (char)rand();
        mac_addr[4] = (char)rand();
        mac_addr[5] = (char)rand();
    }
    else
    {
        ret = ethdev->drv_info->eth_func->mac_get(ethdev, (char *)&mac_addr);
        if (ret != OK)
        {
            KLOG_E("Get MAC Address Error!");
            return -ERROR;
        }
    }

    TTOS_CreateSemaEx(0, &ethdev->rx_sem);
    init_waitqueue_head(&ethdev->wq);
    INIT_SPIN_LOCK(&ethdev->ip_lock);
    INIT_SPIN_LOCK(&ethdev->send_lock);
    TTOS_CreateMutex(1, 0, &ethdev->phy_mutex);

#if CONFIG_LWIP_2_2_0
    netstack_data =
        eth_lwip_device_init(ethdev, drv_info->drv_name, (const uint8_t *const)mac_addr);
    if (NULL == netstack_data)
    {
        KLOG_E("Init lwip netif failed");

        goto err_eth_dev_init;
    }
#endif

    netdev = netdev_install_dev(ethdev, NET_LL_ETHERNET, eth_stack_netdev_ops,
                                eth_netdev_install_cb, netstack_data);
    if (netdev == NULL)
    {
        KLOG_E("Install netdev failed");

        goto err_eth_dev_init;
    }

    ethdev->netdev = netdev;

    /* 注册以太网驱动 */
    eth_drv_register(drv_info);

    /* 将ethdev加入链表 */
    spin_lock_irqsave(&ETH_GLOBAL_LOCK, eth_flags);
    list_add_tail(&ethdev->node, &ETH_DEV_LIST);
    spin_unlock_irqrestore(&ETH_GLOBAL_LOCK, eth_flags);

#ifdef CONFIG_ETH_RX_TASK_BIND_CPU_CORE
    snprintf(rx_task_name, sizeof(rx_task_name), "eth%d_rx_task_c%d", ethdev->eth_unit,
             CONFIG_ETH_RX_TASK_AFFINITY);
#else
    snprintf(rx_task_name, sizeof(rx_task_name), "eth%d_rx_task", ethdev->eth_unit);
#endif
    ret = (int)TTOS_CreateTaskEx((unsigned char *)&rx_task_name, CONFIG_ETH_RX_TASK_PRIORITY, TRUE,
                                 TRUE, (void (*)())eth_rx_task, ethdev,
                                 CONFIG_ETH_RX_TASK_STACK_SIZE, &(ethdev->rx_task));
    if (ret != 0)
    {
        goto err_eth_dev_init;
    }

    /*
      如果是FDT设备，则从设备树节点中读取phy-handle属性的值
      如何是PCIe设备则将phy_handle设置为0xCECECECE
    */
    if (drv_info->bus == fdt)
    {
        ret = of_property_read_u32(device->of_node, "phy-handle", &value);
        if (ret == OK)
        {
            ethdev->phy_handle = value;
        }
        else
        {
            ethdev->phy_handle = 0xDEADBEEF;
            KLOG_W("Can't find phy-handle property!");
        }
    }
    else if (drv_info->bus == pcie)
    {
        ethdev->phy_handle = 0xCECECECE;
    }
    else
    {
        ethdev->phy_handle = 0xDEADBEEF;
    }

    if ((ethdev->drv_info->mdio_func != NULL) &&
        (((ETH_MDIO_READ(ethdev) != NULL) && (ETH_MDIO_WRITE(ethdev) != NULL)) ||
         ((ETH_MDIO45_READ(ethdev) != NULL) && (ETH_MDIO45_WRITE(ethdev) != NULL))))
    {
        /* MAC驱动提供mii_read()/mii_write()，进行PHY设备初始化 */
        ret = eth_drv_param_get(ethdev, "mdio_type", ETH_PARAM_INT32, &param_value);
        ethdev->mdio_type = (ret == 0 ? (MDIO_TYPE)param_value.int32_val : clause22);

        phy_device_init(ethdev, MAC, ethdev->mdio_type, ETH_MDIO_READ(ethdev),
                        ETH_MDIO_WRITE(ethdev), ETH_MDIO45_READ(ethdev), ETH_MDIO45_WRITE(ethdev));
    }
    else if (ethdev->phy_handle != 0xDEADBEEF && ethdev->phy_handle != 0xCECECECE)
    {
        /*
          MAC层未提供mdio读写函数但设置了phy_handle，应由MDIO驱动提供mdio读写函数
          在PHY孤儿设备链表中查询是否有通过MDIO驱动扫描到的设备与MAC匹配，找到则完成连接
        */
        ret = phy_attach_orphan_dev(ethdev);
        if (ret != OK)
        {
            /*
                MAC驱动未实现mdio接口，也为匹配到现有PHY，将MAC设备加入ETH_WAIT_ATTACH_LIST等待与PHY连接
                MDIO驱动初始化时会扫描PHY设备并将其与对应MAC设备关联
            */
            wait_node = (WAIT_NODE *)calloc(1, sizeof(WAIT_NODE));
            if (wait_node == NULL)
            {
                return (-ERROR);
            }

            wait_node->ethdev = ethdev;
            wait_node->phy_handle = ethdev->phy_handle;

            spin_lock_irqsave(&ETH_GLOBAL_LOCK, eth_flags);
            list_add_tail(&wait_node->node, &ETH_WAIT_ATTACH_LIST);
            spin_unlock_irqrestore(&ETH_GLOBAL_LOCK, eth_flags);
        }
    }
    else
    {
        /* MAC驱动既没有提供MDIO读写接口，设备树节点中也没有phy-handle属性 */
        if (drv_info->bus != mmio)
        {
            KLOG_W("Cant't Connect to PHY!");
        }
    }

    atomic_set(&ethdev->refcnt, 1);
    ethdev->init_flag = 0xfeedcafe;

    return OK;

err_eth_dev_init:

    return -ERROR;
}

/* 应通过netdev_uninstall_dev()调用 */
KSYM_EXPORT(eth_device_deinit);
int eth_device_deinit(ETH_DEV *ethdev)
{
    if (ethdev == NULL || ethdev->init_flag != 0xfeedcafe)
    {
        return -ERROR;
    }

    if (atomic_read(&ethdev->refcnt) != 1)
    {
        KLOG_W("eth%d: cannot deinit, refcnt=%d", ethdev->eth_unit,
               atomic_read(&ethdev->refcnt));
        return -EBUSY;
    }

#if CONFIG_LWIP_2_2_0
    struct netif *lwip_netif;
    int i;

    for (i = 0; i < ethdev->lwip_netif_count; i++)
    {
        eth_lwip_del_dev(ethdev->lwip_netif[i]);
    }
#endif

    TTOS_DeleteSema(ethdev->rx_sem);

    /* ETH_IF结构体变量在以太网设备驱动控制结构体中，因此此处仅清空而不是释放 */
    memset(ethdev, 0, sizeof(ETH_DEV));

    eth_dev_count--;

    return OK;
}

void eth_device_loopback_init()
{
    NET_DEV *netdev = NULL;
    void *netstack_data = NULL;
    long eth_flags;

    if (ethdev_lo.init_flag == 0xfeedcafe)
    {
        KLOG_E("LoopBack Device Already Initialized!");
        return;
    }

    ethdev_lo.eth_unit = LOOPBACK_UNIT;
    ethdev_lo.dev_ctrl = NULL;
    ethdev_lo.drv_info = NULL;
    ethdev_lo.device_info = NULL;

    TTOS_CreateSemaEx(0, &ethdev_lo.rx_sem);
    INIT_SPIN_LOCK(&ethdev_lo.ip_lock);
    TTOS_CreateMutex(1, 0, &ethdev_lo.phy_mutex);

#if CONFIG_LWIP_2_2_0
    netstack_data = eth_lwip_loopback_init(&ethdev_lo);
    if (netstack_data == NULL)
    {
        return;
    }
#endif

    netdev = netdev_install_loopback((void *)&ethdev_lo, eth_netdev_install_cb, netstack_data);
    if (netdev == NULL)
    {
        KLOG_E("Install LoopBack Device Failed!");
        return;
    }

    ethdev_lo.netdev = netdev;

    spin_lock_irqsave(&ETH_GLOBAL_LOCK, eth_flags);
    list_add_head(&ethdev_lo.node, &ETH_DEV_LIST);
    spin_unlock_irqrestore(&ETH_GLOBAL_LOCK, eth_flags);

    atomic_set(&ethdev_lo.refcnt, 1);
    ethdev_lo.init_flag = 0xfeedcafe;

    KLOG_I("Install LoopBack Device Success.");
}

#if defined(CONFIG_ETH_RX_TASK_BIND_CPU_CORE)
static int eth_rx_task_affinity_set(ETH_DEV *ethdev, void *arg)
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    int *failure_flag;
    int ret;

    failure_flag = (int *)arg;

    if (!IS_LOOPBACK_DEV(ethdev))
    {
        TTOS_GetTaskAffinity(ethdev->rx_task, &cpuset);
        if (!CPU_ISSET(CONFIG_ETH_RX_TASK_AFFINITY, &cpuset))
        {
            CPU_ZERO(&cpuset);
            CPU_SET(CONFIG_ETH_RX_TASK_AFFINITY, &cpuset);
            ret = TTOS_SetTaskAffinity(ethdev->rx_task, &cpuset);
            if (ret == TTOS_OK)
            {
                KLOG_D("Bind eth%d's RX Task Affinity to CPU%d", ethdev->eth_unit,
                       CONFIG_ETH_RX_TASK_AFFINITY);
                return 0;
            }
            else
            {
                *failure_flag = 1;
            }
        }
    }
}

void eth_task_defer_config()
{
    int tcpip_flag = FALSE;
    int eth_rx_flag = FALSE;
    int failure_flag = 0;

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    int ret;

    while (tcpip_flag == FALSE || eth_rx_flag == FALSE)
    {
        TTOS_SleepTask(TTOS_GetSysClkRate() >> 1);

        if (tcpip_flag == FALSE)
        {
            TTOS_GetTaskAffinity(tcpip_thread_taskid, &cpuset);
            if (!CPU_ISSET(CONFIG_ETH_RX_TASK_AFFINITY, &cpuset))
            {
                CPU_ZERO(&cpuset);
                CPU_SET(CONFIG_ETH_RX_TASK_AFFINITY, &cpuset);
                ret = TTOS_SetTaskAffinity(tcpip_thread_taskid, &cpuset);
                if (ret == TTOS_OK)
                {
                    KLOG_D("Bind TCPIP Thread Affinity to CPU%d", CONFIG_ETH_RX_TASK_AFFINITY);
                    tcpip_flag = TRUE;
                }
            }
            else
            {
                tcpip_flag = TRUE;
            }
        }

        if (eth_rx_flag == FALSE)
        {
            eth_device_iterate(eth_rx_task_affinity_set, &failure_flag);

            /* 可能存在多网卡，因此当有任何一个以太网接收任务亲和力设置失败时都需要再次设置 */
            if (failure_flag == 0)
            {
                eth_rx_flag = TRUE;
            }
            else
            {
                /* failure_flag清零，用于下一次设置亲和力后判断是否有失败发生 */
                failure_flag = 0;
            }
        }
    }

    KLOG_D("Eth Defer Config Task is Over, Bye Bro~");
}
#endif