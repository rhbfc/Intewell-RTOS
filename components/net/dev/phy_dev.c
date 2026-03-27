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
#include <net/ethernet_dev.h>
#include <net/phy_dev.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <ttos_init.h>
#include <shell.h>
#if defined(CONFIG_MONITOR_BY_WORK_QUEUE)
#include <wqueue.h>
#endif

#undef KLOG_TAG
#define KLOG_TAG "PHY"
#include <klog.h>

#define AUTO_NEGO_MAX_NODE_NUMBER 32
#define MII_RE_AUTONEGO_TIMEOUT 10 /* 10s */

/* 系统内PHY设备总数 */
static int phy_dev_count;

/* 完成初始化和与MAC匹配的PHY设备链表 */
static LIST_HEAD(PHY_DEV_LIST);
/* 等待与PHY匹配的MAC设备链表 */
LIST_HEAD(ETH_WAIT_ATTACH_LIST);
/* 未找到对应MAC的PHY设备链表 */
static LIST_HEAD(PHY_ORPHAN_DEV_LIST);
static LIST_HEAD(PHY_DRV_LIST);

DEFINE_SPINLOCK(PHY_DRV_LOCK);
DEFINE_SPINLOCK(PHY_DEV_LOCK);
DEFINE_SPINLOCK(PHY_ORPHAN_DEV_LOCK);

#if defined(CONFIG_MONITOR_BY_WORK_QUEUE)
struct work_s phy_monitor_work;
#endif

static MUTEX_ID reautonego_dev_mutex;
static PHY_DEV *reautonego_dev_list[AUTO_NEGO_MAX_NODE_NUMBER] = {0};
static TASK_ID phy_monitor_task_id;

extern PHY_DRV_INFO generic_phy_drv;
extern int phy_default_media_set(PHY_DEV *phydev, unsigned int media);

/*****************************************************************************************************/

/* 手动实现子节点迭代器(替代 of_get_next_child) */
static struct device_node *get_next_child(const struct device_node *parent,
                                          struct device_node *prev)
{
    struct device_node *next;

    if (!parent)
    {
        return NULL;
    }

    /* 第一个子节点：从 parent->child 开始 */
    if (!prev)
    {
        next = parent->child;
    }
    else
    {
        next = prev->sibling; /* 后续子节点通过 sibling 指针遍历 */
    }

    /* 安全增加引用计数 */
    if (next)
    {
        of_node_get(next);
    }

    /* 减少上一个节点的引用计数 */
    if (prev)
    {
        of_node_put(prev);
    }

    return next;
}

/*
    递归遍历指定设备的所有子节点(包含孙节点)，查找名称中包含"phy"的子节点
    本函数只用于PHY子系统内部，系统实现设备树节点遍历函数时再进行替换
*/
static void find_fdt_phy_node(struct device_node *parent, PHY_DEV *phydev)
{
    struct device_node *child;
    struct device_node *next;
    unsigned int reg;
    unsigned int phandle;
    int ret;

    if (!parent)
    {
        return;
    }

    /* 遍历直接子节点 */
    child = get_next_child(parent, NULL);

    while (child)
    {
        next = get_next_child(parent, child);

        /* 检查节点名称是否包含"phy" */
        if (strstr(child->name, "phy"))
        {
            phydev->device_node = child;
            
            /* 获取 phandle */
            phandle = child->phandle;
            if (phandle == 0)
            {
                goto next_node;
            }

            ret = of_property_read_u32(child, "reg", &reg);
            if (ret == OK)
            {
                /* 如果节点的reg值与PHY设备的地址相同，说明找到了该PHY设备的设备数节点 */
                if (phydev->hw_info.phy_addr == reg)
                {
                    /* 将节点的phandle值保存在PHY_HW_INFO中，用于后续与MAC设备进行匹配 */
                    phydev->hw_info.phandle = phandle;
                }
            }
            else
            {
                printk("Node %p doesn't have reg property, It must be something wrong!\n", child);
                goto next_node;
            }
        }

        /* 递归遍历子节点的子节点 */
        find_fdt_phy_node(child, phydev);

    next_node:
        /* 移动到下一个子节点(get_next_child 已管理引用计数) */
        child = next;
    }
}
/*****************************************************************************************************/

int mdio_read(PHY_DEV *phydev, unsigned char phy_reg, unsigned short *data)
{
    if ((phydev == NULL) || (data == NULL))
    {
        return -1;
    }

    return phydev->mdio_func.mdio_read(phydev->parent_dev, phydev->hw_info.phy_addr, phy_reg,
                                       data);
}

int mdio_write(PHY_DEV *phydev, unsigned char phy_reg, unsigned short data)
{
    if (phydev == NULL)
    {
        return -1;
    }

    return phydev->mdio_func.mdio_write(phydev->parent_dev, phydev->hw_info.phy_addr, phy_reg,
                                        data);
}

int mdio45_read(PHY_DEV *phydev, unsigned char dev_addr, unsigned char phy_reg,
                unsigned short *data)
{
    if ((phydev == NULL) || (data == NULL))
    {
        return -1;
    }

    return phydev->mdio_func.mdio45_read(phydev->parent_dev, phydev->hw_info.phy_addr, dev_addr,
                                         phy_reg, data);
}

int mdio45_write(PHY_DEV *phydev, unsigned char dev_addr, unsigned char phy_reg,
                 unsigned short data)
{
    if (phydev == NULL)
    {
        return -1;
    }

    return phydev->mdio_func.mdio45_write(phydev->parent_dev, phydev->hw_info.phy_addr,
                                          dev_addr, phy_reg, data);
}

int phy_mode_set(PHY_DEV *phydev, unsigned int mode)
{
    int ret;

    if (phydev == NULL)
    {
        return (-1);
    }

    ret = phydev->phy_drv->phy_func->mode_set(phydev, mode);
    if (ret == OK)
    {
        phy_default_media_set(phydev, mode);
        phydev->status &= (unsigned short)~MII_SR_LINK_STATUS;

        return (OK);
    }

    return (-1);
}

/* 用于MAC层更新自身保存的PHY设备状态信息 */
int phy_mode_get(PHY_DEV *phydev, unsigned int *mode, unsigned int *status)
{
    unsigned short old_status;
    int ret;

    if ((phydev == NULL) || (mode == NULL) || (status == NULL))
    {
        return -1;
    }

    if (phydev->hw_info.mdio_type == clause45)
    {
        if (phydev->auto_status == MII_AUTONEG_LINK_CHANGE)
        {
            mdio45_read(phydev, MII45_MMD_AUTONEG, MII45_PCS_STS0, &old_status);
        }
        else
        {
            mdio45_read(phydev, MII45_MMD_PCS, MII45_PCS_STS0, &old_status);
        }
    }
    else
    {
        mdio_read(phydev, MII_STAT_REG, &old_status);
    }

    ret = phydev->phy_drv->phy_func->mode_get(phydev, mode, status);

    if ((old_status & MII_SR_LINK_STATUS) != (phydev->status & MII_SR_LINK_STATUS))
    {
        if (*status & IFM_ACTIVE)
        {
            phydev->status &= (unsigned short)~MII_SR_LINK_STATUS;
        }
        else
        {
            phydev->status |= MII_SR_LINK_STATUS;
        }
    }

    return (ret);
}

/* 通知MAC链路状态发生变化 */
int phy_media_update_notify(PHY_DEV *phydev)
{
    ETH_DEV *ethdev = NULL;
    int ret;

    if (phydev == NULL)
    {
        return (-1);
    }

    ethdev = phydev->ethdev;

    if (ethdev == NULL)
    {
        return (-1);
    }

    if (ethdev->drv_info->eth_func->link_update != NULL)
    {
        ret = ethdev->drv_info->eth_func->link_update(ethdev);

        return (ret);
    }

    return (-1);
}

int phy_default_media_set(PHY_DEV *phydev, unsigned int media)
{
    unsigned int i;

    if (phydev == NULL)
    {
        return (-1);
    }

    for (i = 0; i < phydev->media_list->media_num; i++)
    {
        if (media == phydev->media_list->media[i])
        {
            break;
        }
    }

    if (i == phydev->media_list->media_num)
    {
        return (-1);
    }

    phydev->media_list->default_media = media;

    return (OK);
}

int phy_default_media_get(PHY_DEV *phydev, unsigned int *media)
{
    if (phydev == NULL)
    {
        return (-1);
    }

    *media = phydev->media_list->default_media;

    return OK;
}

int phy_media_list_add(PHY_DEV *phydev, unsigned int media)
{
    MEDIA_LIST *media_list;
    int i;

    if (phydev == NULL)
    {
        return (-1);
    }

    /* 检查列表中是否已经存在要添加的介质 */
    for (i = 0; i < phydev->media_list->media_num; i++)
    {
        if (media == phydev->media_list->media[i])
        {
            return (-1);
        }
    }

    if (phydev->media_list->media_num == 0)
    {
        phydev->media_list->media_num = 1;
        phydev->media_list->media[0] = media;

        return (OK);
    }

    media_list = (MEDIA_LIST *)malloc(sizeof(MEDIA_LIST) +
                                      (sizeof(unsigned int) * (phydev->media_list->media_num)));

    if (media_list == NULL)
    {
        return (-1);
    }

    for (i = 0; i < phydev->media_list->media_num; i++)
    {
        media_list->media[i] = phydev->media_list->media[i];
    }

    media_list->media[i++] = media;

    media_list->media_num = i;
    media_list->default_media = phydev->media_list->default_media;

    free(phydev->media_list);

    phydev->media_list = media_list;

    return (OK);
}

int phy_media_list_del(PHY_DEV *phydev, unsigned int media)
{
    MEDIA_LIST *media_list;
    unsigned int i, j;

    if (phydev == NULL)
    {
        return (-1);
    }

    for (i = 0; i < phydev->media_list->media_num; i++)
    {
        if (media == phydev->media_list->media[i])
        {
            break;
        }
    }

    if (i == phydev->media_list->media_num)
    {
        return (-1);
    }

    if (phydev->media_list->media_num == 1)
    {
        phydev->media_list->media_num = 0;
        phydev->media_list->media[0] = 0;
        phydev->media_list->default_media = 0;

        return (OK);
    }

    media_list = (MEDIA_LIST *)malloc(sizeof(MEDIA_LIST) +
                                      (sizeof(unsigned int) * (phydev->media_list->media_num - 1)));

    if (media_list == NULL)
    {
        return (-1);
    }

    j = 0;

    for (i = 0; i < phydev->media_list->media_num; i++)
    {
        if (phydev->media_list->media[i] != media)
        {
            media_list->media[j] = phydev->media_list->media[i];
            j++;
        }
    }

    media_list->media_num = phydev->media_list->media_num - 1;

    if (phydev->media_list->default_media == media)
    {
        media_list->default_media = 0;
    }
    else
    {
        media_list->default_media = phydev->media_list->default_media;
    }

    free(phydev->media_list);

    phydev->media_list = media_list;

    return (OK);
}

/* 根据驱动名称获取已注册的驱动信息结构体 */
PHY_DRV_INFO *phy_drv_find_by_name(char *drv_name)
{
    PHY_DRV_INFO *drv_info;
    long lock_flags;

    if (drv_name == NULL)
    {
        return NULL;
    }

    spin_lock_irqsave(&PHY_DRV_LOCK, lock_flags);

    list_for_each_entry(drv_info, &PHY_DRV_LIST, node)
    {
        if (strcmp(drv_info->name, drv_name) == 0)
        {
            spin_unlock_irqrestore(&PHY_DRV_LOCK, lock_flags);
            return (drv_info);
        }
    }

    spin_unlock_irqrestore(&PHY_DRV_LOCK, lock_flags);

    return NULL;
}

int phy_id_match(PHY_DEV *phydev, const PHY_ID_ENTRY *id_table, PHY_ID_ENTRY **id_entry)
{
    if (phydev == NULL || id_table == NULL)
    {
        return (-1);
    }

    while (!(id_table->id1 == 0 && id_table->id2 == 0))
    {
        if (id_table->id1 == phydev->hw_info.phy_id1 && id_table->id2 == phydev->hw_info.phy_id2)
        {
            if (id_entry != NULL)
            {
                *id_entry = (PHY_ID_ENTRY *)id_table;
            }

            return OK;
        }

        id_table++;
    }
    
    return (-1);
}

/* use_gen_drv为1时表示匹配不到专有驱动时使用通用PHY驱动 */
int phy_drv_match_dev(PHY_DEV *phydev, int use_gen_drv)
{
    PHY_DRV_INFO *phy_drv = NULL;
    int match_flag = 0;
    int ret;
    long lock_flags;

    if (phydev == NULL)
    {
        return (-1);
    }

    spin_lock_irqsave(&PHY_DRV_LOCK, lock_flags);
    list_for_each_entry(phy_drv, &PHY_DRV_LIST, node)
    {
        /* PHY驱动的probe函数通过phy_id1和phy_id2进行匹配 */
        ret = phy_drv->phy_func->probe(phydev);
        if (ret == 0)
        {
            /* 在循环外部调用PHY驱动的init()函数，尽量缩短自旋锁时间 */
            match_flag = 1;
            break;
        }
    }
    spin_unlock_irqrestore(&PHY_DRV_LOCK, lock_flags);

    if (match_flag == 1)
    {
        /* 调用PHY驱动init()函数 */
        phydev->phy_drv = phy_drv;
        ret = phy_drv->phy_func->init(phydev);
        if (ret != OK)
        {
            return (-1);
        }

        phy_drv->ref_cnt++;
    }
    else if (use_gen_drv)
    {
        /* 未匹配到专用驱动则使用Generic PHY驱动 */
        phydev->phy_drv = &generic_phy_drv;
        generic_phy_drv.phy_func->probe(phydev);
        generic_phy_drv.phy_func->init(phydev);
        generic_phy_drv.ref_cnt++;
    }

    return OK;
}

/* 注册phy设备驱动信息 */
int phy_drv_register(PHY_DRV_INFO *drv)
{
    PHY_DRV_INFO *phydrv = NULL;
    PHY_DEV *phydev = NULL;
    long lock_flags;
    int match_flag = 0;
    int ret;

    if (drv == NULL)
    {
        return (-1);
    }

    spin_lock_irqsave(&PHY_DRV_LOCK, lock_flags);

    list_for_each_entry(phydrv, &PHY_DRV_LIST, node)
    {
        if (strcmp(phydrv->name, drv->name) == 0)
        {
            spin_unlock_irqrestore(&PHY_DRV_LOCK, lock_flags);

            return (-1);
        }
    }

    drv->ref_cnt = 0;

    list_add_tail(&drv->node, &PHY_DRV_LIST);

    spin_unlock_irqrestore(&PHY_DRV_LOCK, lock_flags);

    /* 分别从已连接设备和孤儿设备链表中匹配设备，如果原先使用通用驱动则用专用驱动再次初始化设备 */
    spin_lock_irqsave(&PHY_DEV_LOCK, lock_flags);
    list_for_each_entry(phydev, &PHY_DEV_LIST, node)
    {
        if (phydev->phy_drv != &generic_phy_drv)
        {
            continue;
        }

        /* PHY驱动的probe函数通过phy_id1和phy_id2进行匹配 */
        ret = drv->phy_func->probe(phydev);
        if (ret == 0)
        {
            /* 在循环外部调用PHY驱动的init()函数，尽量缩短自旋锁时间 */
            match_flag = 1;
            break;
        }
    }
    spin_unlock_irqrestore(&PHY_DEV_LOCK, lock_flags);

    if (match_flag == 0)
    {
        spin_lock_irqsave(&PHY_ORPHAN_DEV_LOCK, lock_flags);
        list_for_each_entry(phydev, &PHY_ORPHAN_DEV_LIST, node)
        {
            if (phydev->phy_drv != &generic_phy_drv)
            {
                continue;
            }

            /* PHY驱动的probe函数通过phy_id1和phy_id2进行匹配 */
            ret = drv->phy_func->probe(phydev);
            if (ret == 0)
            {
                /* 在循环外部调用PHY驱动的init()函数，尽量缩短自旋锁时间 */
                match_flag = 1;
                break;
            }
        }
        spin_unlock_irqrestore(&PHY_ORPHAN_DEV_LOCK, lock_flags);
    }

    if (match_flag == 1)
    {
        /* 调用PHY驱动init()函数 */
        phydev->phy_drv = drv;
        drv->phy_func->init(phydev);

        drv->ref_cnt++;
    }

    return (OK);
}

static void phy_make_attach(ETH_DEV *ethdev, PHY_DEV *phydev)
{
    long lock_flags;

    phydev->ethdev = ethdev;
    ethdev->phydev = phydev;
    ethdev->media_list = phydev->media_list;

    /* 设置PHY为默认状态 */
    phy_mode_set(phydev, ethdev->media_list->default_media);

    spin_lock_irqsave(&PHY_DEV_LOCK, lock_flags);
    list_add_tail(&phydev->node, &PHY_DEV_LIST);
    spin_unlock_irqrestore(&PHY_DEV_LOCK, lock_flags);

    phydev->init_flag = PHY_DEV_NORMAL_STATUS;

    KLOG_I("PHY [%x][%x] on addr [%d] attach to ETH %s", phydev->hw_info.phy_id1,
           phydev->hw_info.phy_id2, phydev->hw_info.phy_addr, ethdev->netdev->name);
}

/* 在MAC等待链表中匹配设备，匹配不到则将PHY设备插入孤儿链表 */
static int phy_add_to_orphan_list(PHY_DEV *phydev)
{
    ETH_DEV *ethdev = NULL;
    long lock_flags;

    if (phydev == NULL)
    {
        return (-1);
    }

    phydev->init_flag = PHY_DEV_ORPHAN_STATUS;

    /* 未找到对应的MAC，将PHY加入到孤儿设备链表 */
    spin_lock_irqsave(&PHY_ORPHAN_DEV_LOCK, lock_flags);
    list_add_tail(&phydev->node, &PHY_ORPHAN_DEV_LIST);
    spin_unlock_irqrestore(&PHY_ORPHAN_DEV_LOCK, lock_flags);
    KLOG_I("Insert PHY [%x][%x] on addr [%d] to orphan list", phydev->hw_info.phy_id1,
           phydev->hw_info.phy_id2, phydev->hw_info.phy_addr);

    return (OK);
}

/* 在MAC等待链表中匹配设备，匹配不到则将PHY设备插入孤儿链表 */
int phy_attach_to_eth(PHY_DEV *phydev)
{
    ETH_DEV *ethdev = NULL;
    long lock_flags;

    if (phydev == NULL)
    {
        return (-1);
    }

    ethdev = eth_find_wait_list(phydev->hw_info.phandle);
    if (ethdev != NULL)
    {
        phy_make_attach(ethdev, phydev);

        return (OK);
    }
    else
    {
        /* 未找到对应的MAC，将PHY加入到孤儿设备链表 */
        phy_add_to_orphan_list(phydev);
    }

    return (-1);
}

static PHY_DEV *phy_find_orphan_list(unsigned int phy_handle)
{
    PHY_DEV *phydev = NULL;
    long lock_flags;

    spin_lock_irqsave(&PHY_ORPHAN_DEV_LOCK, lock_flags);

    list_for_each_entry(phydev, &PHY_ORPHAN_DEV_LIST, node)
    {
        /* MAC设备的phy_handle值与PHY设备的节点偏移相同则匹配成功 */
        if (phy_handle == phydev->hw_info.phandle)
        {
            list_delete(&phydev->node);

            return (phydev);
        }
    }

    spin_unlock_irqrestore(&PHY_ORPHAN_DEV_LOCK, lock_flags);

    return (NULL);
}

/* 在PHY孤儿链表中查询是否有ETH_DEV对应的设备，找到则完成连接 */
int phy_attach_orphan_dev(ETH_DEV *ethdev)
{
    PHY_DEV *phydev = NULL;

    if (ethdev == NULL)
    {
        return (-1);
    }

    phydev = phy_find_orphan_list(ethdev->phy_handle);
    if (phydev != NULL)
    {
        phy_make_attach(ethdev, phydev);

        return (OK);
    }

    return (-1);
}

ETH_DEV *eth_find_wait_list(unsigned int phandle)
{
    ETH_DEV *ethdev = NULL;
    WAIT_NODE *wait_node;
    long lock_flags;

    spin_lock_irqsave(&ETH_GLOBAL_LOCK, lock_flags);

    list_for_each_entry(wait_node, &ETH_WAIT_ATTACH_LIST, node)
    {
        /* MAC设备的phy_handle值与PHY设备的节点偏移相同则匹配 */
        if (wait_node->phy_handle == phandle)
        {
            ethdev = wait_node->ethdev;
            list_delete(&wait_node->node);
            free(wait_node);

            return (ethdev);
        }
    }

    spin_unlock_irqrestore(&ETH_GLOBAL_LOCK, lock_flags);

    return (NULL);
}

/*
    Phy设备扫描与初始化
    本函数在MAC驱动初始化时被eth_device_init()调用，因此MAC驱动内无需主动调用本函数；
    如果提供独立的MDIO驱动，则应主动调用本函数
*/
int phy_device_init(void *parent_dev, MDIO_PARENT_TYPE parent_dev_type, MDIO_TYPE mdio_type,
                    MDIO_READ_FUNC mdio_read_func, MDIO_WRITE_FUNC mdio_write_func,
                    MDIO45_READ_FUNC mdio45_read_func, MDIO45_WRITE_FUNC mdio45_write_func)
{
    ETH_DEV *ethdev = NULL;
    unsigned char i;
    unsigned short status;
    unsigned short id1;
    unsigned short id2;
    PHY_DEV *phydev = NULL;
    long lock_flags;
    int match_flag = 0;
    int ret;

    for (i = 0; i < MII_MAX_PHY_NUM; i++)
    {
        status = 0xFFFF;
        id1 = 0xFFFF;
        id2 = 0xFFFF;

        // TODO:查询PHY设备链表中是否已经存在同parent_dev同phy addr的设备，存在即调用continue

        switch (mdio_type)
        {
            case clause22:
                mdio_read_func(parent_dev, i, MII_STAT_REG, &status);
                mdio_read_func(parent_dev, i, MII_STAT_REG, &status);
                mdio_read_func(parent_dev, i, MII_PHY_ID1_REG, &id1);
                mdio_read_func(parent_dev, i, MII_PHY_ID2_REG, &id2);
                break;

            case clause45:
                mdio45_read_func(parent_dev, i, MII45_MMD_PCS, MII_STAT_REG, &status);
                mdio45_read_func(parent_dev, i, MII45_MMD_PCS, MII_STAT_REG, &status);
                mdio45_read_func(parent_dev, i, MII45_MMD_PCS, MII_PHY_ID1_REG, &id1);
                mdio45_read_func(parent_dev, i, MII45_MMD_PCS, MII_PHY_ID2_REG, &id2);
                break;

            default:
                return (-1);
        }

        if (status != 0 && status != 0xFFFF)
        {
            phydev = (PHY_DEV *)calloc(1, sizeof(PHY_DEV));
            if (phydev == NULL)
            {
                return (-1);
            }

            phydev->hw_info.phy_id1 = id1;
            phydev->hw_info.phy_id2 = id2;
            phydev->hw_info.phy_addr = i;
            phydev->hw_info.mdio_type = mdio_type;
            phydev->hw_info.phandle = -1;

            phydev->node = LIST_HEAD_INIT(phydev->node);
            phydev->parent_type = parent_dev_type;
            phydev->parent_dev = parent_dev;
            phydev->mdio_func.mdio_read = mdio_read_func;
            phydev->mdio_func.mdio_write = mdio_write_func;
            phydev->mdio_func.mdio45_read = mdio45_read_func;
            phydev->mdio_func.mdio45_write = mdio45_write_func;

            /*
                设备树文件中，phy作为子节点存在于MAC或MDIO设备节点中，
                在MAC/MDIO设备节点中查找名字中有“phy”的子节点，如果该节点reg属性的值与phy_hw_info->phy_addr相同，
                说明找到了当前PHY设备的节点，然后将节点的phandle值保存在phy_hw_info->fdt_phandle中，
                后续用于与MAC设备进行匹配
            */
            if (parent_dev_type == MAC)
            {
                ethdev = (ETH_DEV *)parent_dev;

                if (ethdev->drv_info->bus == fdt)
                {
                    find_fdt_phy_node(ethdev->device_info->of_node, phydev);
                }
                else if (ethdev->drv_info->bus == pcie)
                {
                    /* PCIe设备设置为0xCECECECE */
                    phydev->hw_info.phandle = 0xCECECECE;

                    /* 检查MAC是否预设了PHY地址 */
                    if (PHY_ADDR_IS_PREASGN(ethdev->drv_info->mdio_func->preasgn_phyaddr))
                    {
                        phydev->hw_info.phy_addr = PHY_ACQUIRE_PREASGN_ADDR(ethdev->drv_info->mdio_func->preasgn_phyaddr);
                        phydev->hw_info.pre_asgn = TRUE;
                    }
                }

                phydev->mdio_func.skip_gen_init = ethdev->drv_info->mdio_func->skip_gen_init;
            }
            else
            {
                // TODO:使用MDIO设备的节点作为参数进行查找
                // find_fdt_phy_node (mdiodev->device->of_node);
            }

            /* 遍历PHY驱动链表查找是否有专用驱动并初始化 */
            ret = phy_drv_match_dev(phydev, TRUE);
            if (ret != OK)
            {
                free(phydev);
                continue;
            }

            /* 没有在设备树中找到当前PHY设备的节点且父设备为MDIO，直接将设备加入孤儿链表 */
            if ((phydev->hw_info.phandle == -1) && (parent_dev_type == MDIO))
            {
                KLOG_W("Find a PHY not be described in Device Tree!\n");

                phy_add_to_orphan_list(phydev);
                goto finish;
            }

            /* 如果父设备是MAC，则可直接完成连接 */
            if (parent_dev_type == MAC)
            {
                /*
                  MAC设备子节点中描述了多个PHY设备且当前设备的phandle与MAC的<phy-handle>属性值不符
                */
                if ((ethdev->phy_handle != 0xDEADBEEF) &&
                    (ethdev->phy_handle != phydev->hw_info.phandle))
                {
                    KLOG_W("Find a PHY [%x][%x] on addr [%d] doesn't match the MAC [%s]!",
                           phydev->hw_info.phy_id1, phydev->hw_info.phy_id2,
                           phydev->hw_info.phy_addr, ethdev->netdev->name);
                    phy_add_to_orphan_list(phydev);

                    goto finish;
                }

                /*
                  ethdev->phy_handle的值可能为0xDEADBEEF，此处主动赋值确保ethdev->phy_handle的正确性
                */
                ethdev->phy_handle = phydev->hw_info.phandle;

                phy_make_attach(ethdev, phydev);

                if (phydev->hw_info.pre_asgn)
                {
                    phydev->unit = phy_dev_count++;
                    break;
                }
            }
            else
            {
                /*
                  父设备为MDIO，从ETH_WAIT_ATTACH_LIST中寻找对应的MAC设备进行连接，未找到则插入PHY孤儿设备链表
                */
                phy_attach_to_eth(phydev);
            }

        finish:
            phydev->unit = phy_dev_count++;
        }
    }

    return (OK);
}

/* 待实现 */
int phy_device_deinit(ETH_DEV *ethdev)
{
    return (OK);
}

static void phy_reautonego_list_add(PHY_DEV *phydev)
{
    int i;

    if (phydev == NULL)
    {
        return;
    }

    TTOS_ObtainMutex(reautonego_dev_mutex, TTOS_WAIT_FOREVER);

    for (i = 0; i < AUTO_NEGO_MAX_NODE_NUMBER; i++)
    {
        if (reautonego_dev_list[i] == NULL)
        {
            reautonego_dev_list[i] = phydev;
            break;
        }
    }

    TTOS_ReleaseMutex(reautonego_dev_mutex);
}

int phy_reautonego_list_find(PHY_DEV *phydev)
{
    int ret;
    int flag = -1;

    if (phydev == NULL)
    {
        return (-1);
    }

    ret = TTOS_ObtainMutex(reautonego_dev_mutex, TTOS_WAIT_FOREVER);
    if (ret != OK)
    {
        return (-1);
    }

    for (int i = 0; i < AUTO_NEGO_MAX_NODE_NUMBER; i++)
    {
        if (reautonego_dev_list[i] == phydev)
        {
            flag = OK;
            break;
        }
    }

    TTOS_ReleaseMutex(reautonego_dev_mutex);

    if (flag != -1)
    {
        flag = OK;
    }

    return flag;
}

/* 等待PHY重新自协商完成，MII_RE_AUTONEGO_TIMEOUT定义的时间长度后删除reautonego_dev_list中第一个设备
 */
static void phy_maintain_reautonego_list()
{
    static T_UDWORD wait_time_1 = 0;
    static unsigned int ref_count_old = 0;
    unsigned int ref_count = 0;
    T_UDWORD wait_time_2 = 0;
    unsigned int wait_temp = 0;
    int ret;
    int i;

    ret = TTOS_ObtainMutex(reautonego_dev_mutex, TTOS_WAIT_FOREVER);
    if (ret != OK)
    {
        return;
    }

    for (i = 0; i < AUTO_NEGO_MAX_NODE_NUMBER; i++)
    {
        if (reautonego_dev_list[i] != NULL)
        {
            ref_count++;
        }
    }

    TTOS_ReleaseMutex(reautonego_dev_mutex);

    if (ref_count_old == 0)
    {
        ref_count_old = ref_count;
    }

    if (ref_count > ref_count_old)
    {
        wait_time_1 = 0;
        ref_count_old = ref_count;
    }

    if (ref_count == 0)
    {
        wait_time_1 = 0;
    }
    else
    {
        if (wait_time_1 == 0)
        {
            TTOS_GetSystemTicks(&wait_time_1);
        }

        TTOS_GetSystemTicks(&wait_time_2);
        wait_temp = (wait_time_2 >= wait_time_1)
                        ? (wait_time_2 - wait_time_1)
                        : ((unsigned long long)0xFFFFFFFFFFFFFFFF - wait_time_1 + wait_time_2);

        if (wait_temp > (MII_RE_AUTONEGO_TIMEOUT * TTOS_GetSysClkRate()))
        {
            TTOS_ObtainMutex(reautonego_dev_mutex, TTOS_WAIT_FOREVER);

            /* 删除第一个设备，并将后面的设备前提 */
            if (reautonego_dev_list[0] != NULL)
            {
                reautonego_dev_list[0] = NULL;

                for (i = 1; i < AUTO_NEGO_MAX_NODE_NUMBER; i++)
                {
                    if (reautonego_dev_list[i] != NULL)
                    {
                        reautonego_dev_list[i - 1] = reautonego_dev_list[i];
                        reautonego_dev_list[i] = NULL;
                    }
                    else
                    {
                        break;
                    }
                }
            }

            TTOS_ReleaseMutex(reautonego_dev_mutex);
            TTOS_GetSystemTicks(&wait_time_1);
        }
    }
}

/* 待实现 */
static void mdio45_monitor(PHY_DEV *phydev)
{
    return;
}

/* 如果PHY是LPI模式，或Local/Remote Receiver状态正常且idle error count为0则返回OK，否则返回-1 */
static int phy_idle_status_inspect(PHY_DEV *phydev, unsigned short status)
{
    // TODO:PHY是LPI模式则直接返回OK

    /* status是MASTER-SLAVE status寄存器的值，低8位为idle error count，寄存器详情见IEEE
     * 802.3规范40.5.1.1章节 */
    if (((status & (MII_MASSLA_STAT_LOCAL_RCV | MII_MASSLA_STAT_REMOTE_RCV)) ==
         (MII_MASSLA_STAT_LOCAL_RCV | MII_MASSLA_STAT_REMOTE_RCV)) &&
        !(status & 0xFF))
    {
        return (OK);
    }

    return (-1);
}

static int phy_idle_error_check(PHY_DEV *phydev)
{
    unsigned short ms_status_1, ms_status_2, ms_status_3;
    unsigned int mode, status;
    unsigned long long time_out;

    if (phydev == NULL)
    {
        return (OK);
    }

    if (phy_mode_get(phydev, &mode, &status) != OK)
    {
        return (-1);
    }

    /* 非千兆双绞线直接返回 */
    if (IFM_SUBTYPE(mode) != IFM_1000_T)
    {
        return (OK);
    }

    /* 检查设备是否已经在重新自协商列表中 */
    if (phy_reautonego_list_find(phydev) != OK)
    {
        time_out = (TTOS_GetSysClkRate() >> 2) + 1;

        /* 通过MASTER-SLAVE status寄存器检查设备是否发生了idle errors */

        mdio_read(phydev, MII_MASSLA_STAT_REG, &ms_status_1);
        if (phy_idle_status_inspect(phydev, ms_status_1) == OK)
        {
            return (OK);
        }

        TTOS_SleepTask(time_out);
        mdio_read(phydev, MII_MASSLA_STAT_REG, &ms_status_2);
        if (phy_idle_status_inspect(phydev, ms_status_2) == OK)
        {
            return (OK);
        }

        TTOS_SleepTask(time_out);
        mdio_read(phydev, MII_MASSLA_STAT_REG, &ms_status_3);
        if (phy_idle_status_inspect(phydev, ms_status_3) == OK)
        {
            return (OK);
        }

        ms_status_1 &= (MII_MASSLA_STAT_LOCAL_RCV | MII_MASSLA_STAT_REMOTE_RCV);
        ms_status_2 &= (MII_MASSLA_STAT_LOCAL_RCV | MII_MASSLA_STAT_REMOTE_RCV);
        ms_status_3 &= (MII_MASSLA_STAT_LOCAL_RCV | MII_MASSLA_STAT_REMOTE_RCV);

        /* 三次状态中有一次正常即可认为PHY可以正常工作 */
        if (ms_status_1 == (MII_MASSLA_STAT_LOCAL_RCV | MII_MASSLA_STAT_REMOTE_RCV) &&
            ms_status_2 == (MII_MASSLA_STAT_LOCAL_RCV | MII_MASSLA_STAT_REMOTE_RCV) &&
            ms_status_3 == (MII_MASSLA_STAT_LOCAL_RCV | MII_MASSLA_STAT_REMOTE_RCV))
        {
            return (OK);
        }

        /* 如果发现错误则使PHY重新进行自协商 */
        mdio_read(phydev, MII_CTRL_REG, &ms_status_1);
        mdio_write(phydev, MII_CTRL_REG, 0);
        ms_status_1 |= MII_CR_RESTART;
        mdio_write(phydev, MII_CTRL_REG, ms_status_1);

        /* 更新系统中PHY的状态 */
        phydev->status &= (unsigned short)~MII_SR_LINK_STATUS;

        /* 将PHY设备加入重新自协商列表中 */
        phy_reautonego_list_add(phydev);
    }

    return (-1);
}

static void phy_monitor_task()
{
    struct list_node *node;
    PHY_DEV *phydev;
    unsigned short mii_status;
    unsigned int media;
    long lock_flags;
    unsigned int delay_ticks = TTOS_GetSysClkRate() * CONFIG_PHY_MONITOR_CHECK_PERIOD;

#if defined(CONFIG_MONITOR_BY_STANDLONE_TASK)
    while (1)
    {
#endif
        spin_lock_irqsave(&PHY_DEV_LOCK, lock_flags);
        node = list_first(&PHY_DEV_LIST);

        while (node != NULL)
        {
            spin_unlock_irqrestore(&PHY_DEV_LOCK, lock_flags);

            phydev = (PHY_DEV *)node;

            if (phydev->hw_info.mdio_type == clause45)
            {
                mdio45_monitor(phydev);
                goto skip;
            }

            mdio_read(phydev, MII_STAT_REG, &mii_status);

            if (phydev->media_list == NULL)
            {
                KLOG_W("PHY Monitor: The media list for the phy of [%s] is empty",
                       phydev->ethdev->netdev->name);

                media = IFM_NONE;
            }
            else
            {
                media = phydev->media_list->default_media;
            }

            if ((mii_status & MII_SR_LINK_STATUS) && !(phydev->status & MII_SR_LINK_STATUS) &&
                IFM_SUBTYPE(media) == IFM_AUTO && !(mii_status & MII_SR_AUTO_NEG))
            {
                /* PHY系统记录状态是DOWN，物理状态是UP，但自协商尚未完成，则跳过继续等待 */
                KLOG_W("PHY Monitor: [%s]'s PHY is link up, but autoneg not complete",
                       phydev->ethdev->netdev->name);

                goto skip;
            }

            if ((mii_status & MII_SR_LINK_STATUS) != (phydev->status & MII_SR_LINK_STATUS))
            {
                /* PHY系统记录状态与物理状态不符 */
                KLOG_I("PHY Monitor: Link status of the PHY of [%s] has changed",
                       phydev->ethdev->netdev->name);

                // TODO:检查PHY是否是LPI模式

                /* 更新PHY状态 */
                mdio_read(phydev, MII_STAT_REG, &phydev->status);

                /* 检查UP状态的PHY设备是否发生idle errors */
                if (phydev->status & MII_SR_LINK_STATUS)
                {
                    KLOG_I("PHY Monitor: [%s]'s PHY is Link UP now", phydev->ethdev->netdev->name);

                    if (phy_idle_error_check(phydev) == -1)
                    {
                        phydev->status &= (unsigned short)~MII_SR_LINK_STATUS;
                        KLOG_E("PHY Monitor: Idel errors has found in the PHY of [%s]",
                               phydev->ethdev->netdev->name);

                        goto skip;
                    }
                }
                else
                {
                    KLOG_I("PHY Monitor: [%s]'s PHY is Link DOWN now",
                           phydev->ethdev->netdev->name);
                }

                /* 通知MAC设备PHY状态发生变化 */
                phy_media_update_notify(phydev);
            }

        skip:
            spin_lock_irqsave(&PHY_DEV_LOCK, lock_flags);
            node = list_next(node, &PHY_DEV_LIST);
        }

        spin_unlock_irqrestore(&PHY_DEV_LOCK, lock_flags);

        /* 检查并维护重协商设备表reautonego_dev_list */
        phy_maintain_reautonego_list();

#if defined(CONFIG_MONITOR_BY_STANDLONE_TASK)
        TTOS_SleepTask(delay_ticks);
    }
#elif defined(CONFIG_MONITOR_BY_WORK_QUEUE)
        work_queue(&phy_monitor_work, phy_monitor_task, NULL, delay_ticks, NULL);
#endif
}

void phy_monitor_task_create()
{
#if defined(CONFIG_MONITOR_BY_WORK_QUEUE)
    work_queue(&phy_monitor_work, phy_monitor_task, NULL, 0, NULL);
    KLOG_I("Add PHY Monitor to Work Queue");
#elif defined(CONFIG_MONITOR_BY_STANDLONE_TASK)
    char task_name[32];
    int ret;

    snprintf(task_name, sizeof(task_name), "phy_monitor");
    ret = (int)TTOS_CreateTaskEx((unsigned char *)&task_name, CONFIG_PHY_MONITOR_TASK_PRIORITY,
                                 TRUE, TRUE, (void (*)())phy_monitor_task, NULL,
                                 DEFAULT_TASK_STACK_SIZE, &phy_monitor_task_id);
    if (ret != 0)
    {
        KLOG_E("Create PHY Monitor Task Failed!");
    }
    else
    {
        KLOG_I("Create PHY Monitor Task");
    }
#endif
}

int phy_subsystem_init()
{
    int i;

    TTOS_CreateMutex(1, 0, &reautonego_dev_mutex);

    for (i = 0; i < AUTO_NEGO_MAX_NODE_NUMBER; i++)
    {
        reautonego_dev_list[i] = NULL;
    }

    phy_monitor_task_create();
    return 0;
}

INIT_EXPORT_COMPONENTS(phy_subsystem_init, "PHY Subsystem Init");

#ifdef CONFIG_SHELL
static int phy_info(int argc, char *argv[])
{
    PHY_DRV_INFO *phydrv = NULL;
    PHY_DEV *phydev = NULL;
    long lock_flags;

    printk("PHY Driver Info:\n");
    printk("Driver:[%s] Info:[%s] Refs:[%d]\n", generic_phy_drv.name,  generic_phy_drv.desc, generic_phy_drv.ref_cnt);

    list_for_each_entry(phydrv, &PHY_DRV_LIST, node)
    {
        printk("Driver:[%s] Info:[%s] Refs:[%d]\n", phydrv->name,  phydrv->desc, phydrv->ref_cnt);
    }

    printk("\nPHY Attached Device:\n");

    spin_lock_irqsave(&PHY_DEV_LOCK, lock_flags);
    list_for_each_entry(phydev, &PHY_DEV_LIST, node)
    {
        printk("[%02d]    ID1:[%x] ID2:[%x] Addr:[%d] Phandle:[0x%x]\n", phydev->unit, phydev->hw_info.phy_id1, phydev->hw_info.phy_id2, phydev->hw_info.phy_addr, phydev->hw_info.phandle);
        printk("        Driver:[%s] MAC:[%s] Parent:[%s]\n\n", phydev->phy_drv->name, phydev->ethdev->netdev->name, phydev->parent_type == MAC ? "MAC" : "MDIO");
    }
    spin_unlock_irqrestore(&PHY_DEV_LOCK, lock_flags);

    printk("PHY Orphan Device:\n");

    spin_lock_irqsave(&PHY_ORPHAN_DEV_LOCK, lock_flags);
    list_for_each_entry(phydev, &PHY_ORPHAN_DEV_LIST, node)
    {
        printk("[%02d]    ID1:[%x] ID2:[%x] Addr:[%d] Phandle:[0x%x]\n", phydev->unit, phydev->hw_info.phy_id1, phydev->hw_info.phy_id2, phydev->hw_info.phy_addr, phydev->hw_info.phandle);
        printk("        Driver:[%s] Parent:[%s]\n\n", phydev->phy_drv->name, phydev->parent_type == MAC ? "MAC" : "MDIO");
    }
    spin_unlock_irqrestore(&PHY_ORPHAN_DEV_LOCK, lock_flags);

    return 0;
}

SHELL_EXPORT_CMD (SHELL_CMD_PERMISSION (0)
                      | SHELL_CMD_TYPE (SHELL_TYPE_CMD_MAIN)
                      | SHELL_CMD_DISABLE_RETURN,
                  phyinfo, phy_info, PHY Infomation);
#endif