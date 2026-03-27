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

#ifndef __DEVICE_H__
#define __DEVICE_H__
#include <kobject.h>
#include <limits.h>
#include <linux/klist.h>
#include <list.h>
#include <spinlock.h>
#include <stdarg.h>
#include <system/gfp_types.h>
#include <system/types.h>

struct driver;
struct device_node;
struct devclass_type;
struct bus_type;
struct of_phandle_args;

typedef struct devaddr_region
{
    phys_addr_t paddr;
    size_t size;
    virt_addr_t vaddr;
} devaddr_region_t;

struct device_private
{
    struct klist klist_children;
    struct klist_node knode_parent;
    struct klist_node knode_driver;
    struct klist_node knode_bus;
    struct klist_node knode_class;
    struct list_head deferred_probe;
    const struct device_driver *async_driver;
    char *deferred_probe_reason;
    struct device *device;
    u8 dead : 1;
};
#define to_device_private_parent(obj) container_of(obj, struct device_private, knode_parent)
#define to_device_private_driver(obj) container_of(obj, struct device_private, knode_driver)
#define to_device_private_bus(obj) container_of(obj, struct device_private, knode_bus)
#define to_device_private_class(obj) container_of(obj, struct device_private, knode_class)

struct device
{
    const char *init_name; /* initial name of the device */

    struct kobject kobj;

    struct list_head devres_head;
    struct driver *driver;
    struct device_node *of_node;
    struct device *parent;
    int major;
    int minor;
    int is_fixed_minor;

    struct device_private *p;

    ttos_spinlock_t devres_lock;

    char name[NAME_MAX];

    /* 从属的bus */
    const struct bus_type *bus;

    /* 从属的class */
    struct devclass_type *class;

    /* 设备私有数据 */
    void *priv;

    /* platdorm bus私有数据 */
    void *platdata;

    /* 驱动私有数据 */
    const void *driver_data;

    /* 设备资源释放函数 */
    void (*release)(struct device *);
};

#define kobj_to_dev(__kobj) container_of(__kobj, struct device, kobj)

#define is_device_drivered(device) ((device)->driver != NULL)

#define dev_priv_get(dev) ((dev)->priv)
#define dev_priv_set(dev, parm) ((dev)->priv = (void *)parm)

struct pci_device_id
{
    uint32_t vendor;
    uint32_t device;
    uint32_t subvendor;
    uint32_t subdevice;
    uint32_t class;
    uint32_t class_mask;
    void *driver_data;
};

/**
 * dev_name - Return a device's name.
 * @dev: Device with name to get.
 * Return: The kobject name of the device, or its initial name if unavailable.
 */
static inline const char *dev_name(const struct device *dev)
{
    return dev->name;
}

int devbus_register_device(struct device *dev);

void initialize_device(struct device *dev);

int register_device(struct device *dev);

struct device *device_find_byname(char *name);

struct device *get_device(struct device *dev);
void put_device(struct device *dev);

/* device resource management */
#define NUMA_NO_NODE (-1)
#define NUMA_NO_MEMBLK (-1)

/**
 * @brief 资源释放函数类型
 * @param dev 设备指针
 * @param res 资源数据指针
 */
typedef void (*dr_release_t)(struct device *dev, void *res);

/**
 * @brief 分配设备资源（内部实现，通过宏调用）
 */
void *__devres_alloc_node(dr_release_t release, size_t size, gfp_t gfp, int nid, const char *name);

/**
 * @brief 分配设备资源
 * @param release 资源释放函数
 * @param size 资源数据大小
 * @param gfp 分配标志
 * @return 资源数据指针，失败返回 NULL
 */
#define devres_alloc(release, size, gfp)                                                           \
    __devres_alloc_node(release, size, gfp, NUMA_NO_NODE, #release)

/**
 * @brief 释放设备资源数据
 */
void devres_free(void *res);

/**
 * @brief 将资源添加到设备
 */
void devres_add(struct device *dev, void *res);

/**
 * @brief 释放设备的所有托管资源
 * @return 释放的资源数量
 */
int devres_release_all(struct device *dev);

/* 托管内存 */
void *devm_kmalloc(struct device *dev, size_t size, gfp_t gfp);
void *devm_kzalloc(struct device *dev, size_t size, gfp_t gfp);
void devm_kfree(struct device *dev, const void *p);
char *devm_kstrdup(struct device *dev, const char *s, gfp_t gfp);

/* 托管 I/O 内存映射 */
volatile void *devm_ioremap(struct device *dev, phys_addr_t addr, size_t size);
void devm_iounmap(struct device *dev, volatile void *addr);

/* 托管中断 */
int devm_request_irq(struct device *dev, unsigned int irq, void (*handler)(unsigned int, void *),
                     unsigned long irqflags, const char *devname, void *dev_id);

void *dev_get_priv(struct device *dev);
void *dev_get_platdata(struct device *dev);
const void *dev_get_driver_data(struct device *dev);
uint32_t dev_read_u32_default(struct device *dev, const char *property, uint32_t default_value);
int dev_read_phandle_with_args(struct device *dev, const char *list_name, const char *cells_name,
                               int cell_count, int index, struct of_phandle_args *out_args);

int dev_property_read_string_index(struct device *dev, const char *propname, int index,
                                   const char **output);

int dev_read_u32_array(struct device *dev, const char *propname, uint32_t *out_values, size_t sz);
const char *dev_read_string(struct device *dev, const char *propname);
volatile void *dev_read_addr(struct device *dev);
bool dev_read_bool(struct device *dev, const char *propname);

#endif /* __DEVICE_H__ */
