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

#ifndef __DEVBUS_H__
#define __DEVBUS_H__

#include <kobject.h>
#include <linux/klist.h>
#include <list.h>
#include <notifier.h>
#include <spinlock.h>
#include <ttos.h>

struct device;
struct driver;
struct device_node;
struct ttos_pic;
struct platform_driver;
typedef struct devaddr_region devaddr_region_t;
struct kobj_uevent_env;
struct bus_type;

struct subsys_private
{
    struct kset subsys;
    struct kset *devices_kset;
    struct list_head interfaces;
    MUTEX_ID mutex;

    struct kset *drivers_kset;
    struct klist klist_devices;
    struct klist klist_drivers;
    struct blocking_notifier_head bus_notifier;
    unsigned int drivers_autoprobe : 1;
    const struct bus_type *bus;
    struct device *dev_root;

    struct kset glue_dirs;
    // const struct class *class;

    // struct lock_class_key lock_key;
};
#define to_subsys_private(obj) container_of(obj, struct subsys_private, subsys.kobj)

static inline struct subsys_private *subsys_get(struct subsys_private *sp)
{
    if (sp)
        kset_get(&sp->subsys);
    return sp;
}

static inline void subsys_put(struct subsys_private *sp)
{
    if (sp)
        kset_put(&sp->subsys);
}

// struct subsys_private *class_to_subsys(const struct class *class);

struct driver_private
{
    struct kobject kobj;
    struct klist klist_devices;
    struct klist_node knode_bus;
    // struct module_kobject *mkobj;
    struct driver *driver;
};
#define to_driver(obj) container_of(obj, struct driver_private, kobj)

/**
 * struct bus_type - 设备总线类型
 *
 * @name:	总线名称。
 * @dev_name:	用于子系统枚举设备，如 ("foo%u", dev->id) 格式。
 * @bus_groups:	总线默认属性组。
 * @dev_groups:	总线上设备的默认属性组。
 * @drv_groups:	总线上驱动程序的默认属性组。
 * @match:	当新设备或驱动程序添加到此总线时调用（可能多次）。如果给定设备
 *		可以由给定驱动程序处理，则返回正值；否则返回零。如果无法确定驱动
 *		程序是否支持该设备，也可能返回错误代码。如果返回 -EPROBE_DEFER，
 *		则将设备加入延迟探测队列。
 * @uevent:	当设备添加、移除或发生其他会生成 uevent 的事件时调用，用于添加
 *		环境变量。
 * @probe:	当新设备或驱动程序添加到此总线时调用，会回调特定驱动程序的 probe
 *		函数以初始化匹配的设备。
 * @sync_state:	在设备的所有状态跟踪消费者（在 late_initcall 时存在）成功绑定到
 *		驱动程序后，调用此函数同步设备状态到软件状态。如果设备没有消费者，
 *		此函数将在 late_initcall_sync 级别被调用。如果设备有永远不会绑定
 *		到驱动程序的消费者，此函数将永远不会被调用。
 * @remove:	当设备从此总线移除时调用。
 * @shutdown:	在系统关闭时调用，用于使设备进入静默状态。
 * @irq_get_affinity:	获取此总线上设备的中断亲和性掩码。
 *
 * @online:	使设备重新上线时调用（在设备离线后）。
 * @offline:	使设备离线以进行热插拔时调用，可能失败。
 *
 * @suspend:	当此总线上的设备要进入睡眠模式时调用。
 * @resume:	当此总线上的设备需要从睡眠模式唤醒时调用。
 * @num_vf:	调用以查询此总线上设备支持的虚拟功能数量。
 * @dma_configure:	调用以设置此总线上设备的 DMA 配置。
 * @dma_cleanup:	调用以清理此总线上设备的 DMA 配置。
 * @pm:		此总线的电源管理操作，会回调特定设备驱动程序的 pm-ops。
 * @need_parent_lock:	当在此总线上探测或移除设备时，设备核心应该锁定设备的
 *			父设备。
 *
 * 总线是处理器和一个或多个设备之间的通道。就设备模型而言，所有设备都通过总线连接，
 * 即使是内部的、虚拟的"平台"总线。总线之间可以相互连接，例如 USB 控制器通常是一个
 * PCI 设备。设备模型表示总线与其控制的设备之间的实际连接关系。总线由 bus_type
 * 结构体表示，其中包含名称、默认属性、总线方法、电源管理操作以及驱动程序核心的
 * 私有数据。
 */
struct bus_type
{
    const char *name;
    const char *dev_name;
    const struct attribute_group **bus_groups;
    const struct attribute_group **dev_groups;
    const struct attribute_group **drv_groups;

    int (*match)(struct device *dev, const struct driver *drv);
    int (*uevent)(const struct device *dev, struct kobj_uevent_env *env);
    int (*probe)(struct device *dev);
    void (*sync_state)(struct device *dev);
    void (*remove)(struct device *dev);
    void (*shutdown)(struct device *dev);
    const cpu_set_t *(*irq_get_affinity)(struct device *dev, unsigned int irq_vec);

    int (*online)(struct device *dev);
    int (*offline)(struct device *dev);

    // int (*suspend)(struct device *dev, pm_message_t state);
    // int (*resume)(struct device *dev);

    int (*num_vf)(struct device *dev);

    int (*dma_configure)(struct device *dev);
    void (*dma_cleanup)(struct device *dev);

    // const struct dev_pm_ops *pm;

    bool need_parent_lock;

    /* Private data - used by bus core */
    struct subsys_private *p;
};

struct bus_attribute
{
    struct attribute attr;
    ssize_t (*show)(const struct bus_type *bus, char *buf);
    ssize_t (*store)(const struct bus_type *bus, const char *buf, size_t count);
};

#define to_bus_attr(_attr) container_of(_attr, struct bus_attribute, attr)

#define BUS_ATTR_RW(_name) struct bus_attribute bus_attr_##_name = __ATTR_RW(_name)
#define BUS_ATTR_RO(_name) struct bus_attribute bus_attr_##_name = __ATTR_RO(_name)
#define BUS_ATTR_WO(_name) struct bus_attribute bus_attr_##_name = __ATTR_WO(_name)

static struct subsys_private *bus_to_subsys(const struct bus_type *bus);

/* 注册设备总线 */
__must_check int devbus_register(const struct bus_type *bus);

/* 注销设备总线 */
int devbus_unregister(const struct bus_type *bus);

/* 向总线添加驱动 */
int devbus_add_driver(const struct bus_type *bus, struct driver *drv);

/* 向总线移除驱动 */
int devbus_remove_driver(struct driver *drv);

/* 驱动匹配 */
int devbus_match_driver(struct device *dev, struct driver *drv);

/* 设备实例化 */
int devbus_probe_device(struct device *dev);

/* 移除设备 */
int devbus_remove_device(struct device *dev);

/* 通过name搜索总线上的设备 */
struct device *devbus_find_device_byname(char *name);

/* 搜索指定的 BUS */
struct bus_type *devbus_get_bus(uint32_t type_id, uint32_t bus_id);

/* 遍历总线上的设备 */
int devbus_foreach_dev(const struct bus_type *bus, int (*func)(struct device *dev, void *data),
                       void *data);

int devbus_foreach_driver(const struct bus_type *bus, int (*func)(struct driver *drv, void *data),
                          void *data);

int of_device_get_resource_reg(struct device *dev, phys_addr_t *reg_addr, size_t *reg_size);
int of_device_get_resource_regs(struct device *dev, devaddr_region_t *region, size_t region_count);
int of_device_get_resource_pic_irq(struct device *dev, struct ttos_pic **pic, int *irq_no,
                                   int irq_no_count, int *irq_count);

#endif /* __DEVBUS_H__ */
