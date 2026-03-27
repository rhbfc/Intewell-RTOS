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

#include <driver/device.h>
#include <errno.h>
#include <string.h>
#include <system/gfp_types.h>
#include <ttosMM.h>
#include <ttos_pic.h>

/**
 * @brief 设备资源节点
 *
 * 每个托管资源都对应一个 devres 节点，包含释放函数和实际数据
 */
struct devres
{
    struct list_head entry;                         /**< 链表节点 */
    void (*release)(struct device *dev, void *res); /**< 资源释放函数 */
    size_t size;                                    /**< 数据大小 */
    uint64_t data[] __attribute__((aligned(8)));    /**< 实际数据区域 */
};

/**
 * @brief 从 devres 结构获取数据指针
 * @param dr devres 结构指针
 * @return 数据区域指针
 */
static inline void *devres_data(struct devres *dr)
{
    return (void *)dr->data;
}

/**
 * @brief 分配 devres 结构
 * @param release 资源释放函数
 * @param size 数据区域大小
 * @param gfp 分配标志
 * @return devres 指针，失败返回 NULL
 */
static struct devres *alloc_dr(void (*release)(struct device *, void *), size_t size, gfp_t gfp)
{
    struct devres *dr;
    size_t total_size = sizeof(struct devres) + size;

    dr = (struct devres *)malloc(total_size);
    if (!dr)
        return NULL;

    memset(dr, 0, total_size);
    INIT_LIST_HEAD(&dr->entry);
    dr->release = release;
    dr->size = size;

    return dr;
}

/**
 * @brief 释放 devres 结构
 * @param dr devres 结构指针
 */
static void free_dr(struct devres *dr)
{
    if (dr)
        free(dr);
}

/**
 * @brief 分配设备资源数据（内部实现）
 * @param release 资源释放函数
 * @param size 分配大小
 * @param gfp 分配标志
 * @param nid NUMA 节点（TTOS 中忽略）
 * @param name 资源名称（用于调试）
 * @return 分配的资源数据指针，失败返回 NULL
 */
void *__devres_alloc_node(dr_release_t release, size_t size, gfp_t gfp, int nid, const char *name)
{
    struct devres *dr;

    /* NUMA 节点在 TTOS 中忽略 */
    dr = alloc_dr(release, size, gfp);
    if (!dr)
        return NULL;

    return devres_data(dr);
}

/**
 * @brief 释放设备资源数据
 * @param res 资源数据指针
 *
 * 释放通过 devres_alloc() 分配的资源数据
 */
void devres_free(void *res)
{
    if (res)
    {
        struct devres *dr = container_of(res, struct devres, data);
        free_dr(dr);
    }
}

/**
 * @brief 注册设备资源
 * @param dev 设备指针
 * @param res 资源数据指针
 *
 * 将资源注册到设备，驱动卸载时自动释放
 */
void devres_add(struct device *dev, void *res)
{
    struct devres *dr = container_of(res, struct devres, data);
    unsigned long flags;

    spin_lock_irqsave(&dev->devres_lock, flags);
    list_add_tail(&dr->entry, &dev->devres_head);
    spin_unlock_irqrestore(&dev->devres_lock, flags);
}

/**
 * @brief 释放设备的所有资源
 * @param dev 设备指针
 * @return 释放的资源数量
 *
 * 按 LIFO 顺序（分配的逆序）释放所有托管资源
 * 在驱动卸载时被自动调用
 */
int devres_release_all(struct device *dev)
{
    struct devres *dr, *tmp;
    unsigned long flags;
    int count = 0;

    spin_lock_irqsave(&dev->devres_lock, flags);

    /* 按 LIFO 顺序释放（分配的逆序） */
    list_for_each_entry_safe_reverse(dr, tmp, &dev->devres_head, entry)
    {
        list_del_init(&dr->entry);
        spin_unlock_irqrestore(&dev->devres_lock, flags);

        /* 在锁外调用释放函数 */
        if (dr->release)
            dr->release(dev, devres_data(dr));

        free_dr(dr);
        count++;

        spin_lock_irqsave(&dev->devres_lock, flags);
    }

    spin_unlock_irqrestore(&dev->devres_lock, flags);

    return count;
}

/**
 * @brief 托管内存释放回调
 */
static void devm_kmalloc_release(struct device *dev, void *res)
{
    /* 内存在 devres 节点释放时一并释放 */
}

/**
 * @brief 托管内存分配
 * @param dev 设备指针
 * @param size 分配大小
 * @param gfp 分配标志
 * @return 分配的内存指针，失败返回 NULL
 *
 * 分配的内存在驱动卸载时自动释放
 */
void *devm_kmalloc(struct device *dev, size_t size, gfp_t gfp)
{
    void *ptr;

    if (size == 0)
        return NULL;

    ptr = devres_alloc(devm_kmalloc_release, size, gfp);
    if (!ptr)
        return NULL;

    devres_add(dev, ptr);
    return ptr;
}

/**
 * @brief 托管内存分配（清零）
 * @param dev 设备指针
 * @param size 分配大小
 * @param gfp 分配标志
 * @return 分配的内存指针，失败返回 NULL
 */
void *devm_kzalloc(struct device *dev, size_t size, gfp_t gfp)
{
    return devm_kmalloc(dev, size, gfp | __GFP_ZERO);
}

/**
 * @brief 释放托管内存
 * @param dev 设备指针
 * @param p 内存指针
 *
 * 提前释放通过 devm_kmalloc() 分配的内存
 */
void devm_kfree(struct device *dev, const void *p)
{
    struct devres *dr, *tmp;
    unsigned long flags;

    if (!p)
        return;

    spin_lock_irqsave(&dev->devres_lock, flags);

    list_for_each_entry_safe(dr, tmp, &dev->devres_head, entry)
    {
        if (devres_data(dr) == p)
        {
            list_del_init(&dr->entry);
            spin_unlock_irqrestore(&dev->devres_lock, flags);
            free_dr(dr);
            return;
        }
    }

    spin_unlock_irqrestore(&dev->devres_lock, flags);
}

/**
 * @brief I/O 映射数据结构
 */
struct devm_ioremap_data
{
    volatile void *addr; /**< 虚拟地址 */
    size_t size;         /**< 映射大小 */
};

/**
 * @brief I/O 映射释放回调
 */
static void devm_ioremap_release(struct device *dev, void *res)
{
    struct devm_ioremap_data *data = res;
    if (data->addr)
        iounmap((virt_addr_t)data->addr);
}

/**
 * @brief 托管 I/O 内存映射
 * @param dev 设备指针
 * @param phys_addr 物理地址
 * @param size 映射大小
 * @return 虚拟地址，失败返回 NULL
 *
 * 映射的 I/O 内存在驱动卸载时自动取消映射
 */
volatile void *devm_ioremap(struct device *dev, phys_addr_t phys_addr, size_t size)
{
    struct devm_ioremap_data *data;
    volatile void *addr;

    addr = ioremap(phys_addr, size);
    if (!addr)
        return NULL;

    data = devres_alloc(devm_ioremap_release, sizeof(*data), GFP_KERNEL);
    if (!data)
    {
        iounmap((virt_addr_t)addr);
        return NULL;
    }

    data->addr = addr;
    data->size = size;

    devres_add(dev, data);
    return addr;
}

/**
 * @brief 取消 I/O 内存映射
 * @param dev 设备指针
 * @param addr 虚拟地址
 *
 * 提前取消通过 devm_ioremap() 创建的映射
 */
void devm_iounmap(struct device *dev, volatile void *addr)
{
    struct devres *dr, *tmp;
    unsigned long flags;

    if (!addr)
        return;

    spin_lock_irqsave(&dev->devres_lock, flags);

    list_for_each_entry_safe(dr, tmp, &dev->devres_head, entry)
    {
        if (dr->release == devm_ioremap_release)
        {
            struct devm_ioremap_data *data = devres_data(dr);
            if (data->addr == addr)
            {
                list_del_init(&dr->entry);
                spin_unlock_irqrestore(&dev->devres_lock, flags);

                iounmap((virt_addr_t)data->addr);
                free_dr(dr);
                return;
            }
        }
    }

    spin_unlock_irqrestore(&dev->devres_lock, flags);
}

/**
 * @brief 中断资源数据结构
 */
struct devm_irq_data
{
    unsigned int irq; /**< 中断号 */
    char *name;       /**< 中断名称 */
};

/**
 * @brief 中断资源释放回调
 */
static void devm_irq_release(struct device *dev, void *res)
{
    struct devm_irq_data *data = res;
    ttos_pic_irq_uninstall(data->irq, data->name);
    if (data->name)
        free(data->name);
}

/**
 * @brief 托管中断请求
 * @param dev 设备指针
 * @param irq 中断号
 * @param handler 中断处理函数（TTOS 签名：void (*)(unsigned int, void*)）
 * @param irqflags 中断标志
 * @param devname 设备名称
 * @param dev_id 传递给处理函数的设备ID
 * @return 0 成功，负数表示错误码
 *
 * 注册的中断在驱动卸载时自动释放
 */
int devm_request_irq(struct device *dev, unsigned int irq, void (*handler)(unsigned int, void *),
                     unsigned long irqflags, const char *devname, void *dev_id)
{
    struct devm_irq_data *data;
    int ret;

    /* TTOS 使用 ttos_pic_irq_install，函数签名为 isr_handler_t */
    ret = ttos_pic_irq_install(irq, (isr_handler_t)handler, dev_id, irqflags, devname);
    if (ret)
        return ret;

    data = devres_alloc(devm_irq_release, sizeof(*data), GFP_KERNEL);
    if (!data)
    {
        ttos_pic_irq_uninstall(irq, devname);
        return -ENOMEM;
    }

    data->irq = irq;
    /* 手动复制名称字符串 */
    if (devname)
    {
        size_t len = strlen(devname) + 1;
        data->name = malloc(len);
        if (data->name)
            memcpy(data->name, devname, len);
    }
    else
    {
        data->name = NULL;
    }

    devres_add(dev, data);
    return 0;
}

/**
 * @brief 托管字符串复制
 * @param dev 设备指针
 * @param s 源字符串
 * @param gfp 分配标志
 * @return 复制的字符串指针，失败返回 NULL
 *
 * 复制的字符串在驱动卸载时自动释放
 */
char *devm_kstrdup(struct device *dev, const char *s, gfp_t gfp)
{
    char *buf;
    size_t len;

    if (!s)
        return NULL;

    len = strlen(s) + 1;
    buf = devm_kmalloc(dev, len, gfp);
    if (buf)
        memcpy(buf, s, len);

    return buf;
}
