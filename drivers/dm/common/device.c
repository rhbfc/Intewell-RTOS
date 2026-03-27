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

#include <driver/devbus.h>
#include <driver/devclass.h>
#include <driver/device.h>
#include <driver/of.h>
#include <errno.h>
#include <fs/fs.h>
#include <inttypes.h>
#include <kmalloc.h>
#include <string.h>
#include <ttosMM.h>
#include <ttos_init.h>

#define KLOG_TAG "DEVICE"
#include <klog.h>

/* Compatibility macros for memory allocation */
#define kzalloc(size, flags) calloc(1, size)
#define kfree(ptr) free(ptr)

struct kset *devices_kset;
static struct kobject *dev_kobj;
/* /sys/dev/char */
static struct kobject *sysfs_dev_char_kobj;

/* /sys/dev/block */
static struct kobject *sysfs_dev_block_kobj;

void initialize_device(struct device *dev)
{
    if (!dev)
    {
        return;
    }

    /* 初始化 device_private 结构体 */
    if (!dev->p)
    {
        dev->p = kzalloc(sizeof(struct device_private), GFP_KERNEL);
        if (!dev->p)
        {
            KLOG_E("Failed to allocate device_private");
            return;
        }
        dev->p->device = dev;
    }

    /* 初始化 kobject - 嵌入式结构体，只需初始化字段 */
    memset(&dev->kobj, 0, sizeof(struct kobject));

    /* 初始化托管资源 */
    INIT_LIST_HEAD(&dev->devres_head);
    spin_lock_init(&dev->devres_lock);
}

int register_device(struct device *dev)
{
    int ret = -EFAULT;
    if (dev && dev->bus)
    {
        ret = devbus_register_device(dev);
        if (ret)
            return ret;
    }

    return ret;
}

struct device *get_device(struct device *dev)
{
    if (dev)
    {
        kobject_get(&dev->kobj);
        return dev;
    }
    return NULL;
}

void put_device(struct device *dev)
{
    /* might_sleep(); */
    if (dev)
        kobject_put(&dev->kobj);
}

struct device *device_find_byname(char *name)
{
    return devbus_find_device_byname(name);
}

void *dev_get_priv(struct device *dev)
{
    if (!dev)
    {
        KLOG_D("%s: null device", __func__);
        return NULL;
    }

    return dev->priv;
}

void *dev_get_platdata(struct device *dev)
{
    if (!dev)
    {
        KLOG_D("%s: null device\n", __func__);
        return NULL;
    }

    return dev->platdata;
}

const void *dev_get_driver_data(struct device *dev)
{
    return dev->driver_data;
}

uint32_t dev_read_u32_default(struct device *dev, const char *property, uint32_t default_value)
{
    return ofnode_read_u32_default(dev->of_node, property, default_value);
}

int dev_read_phandle_with_args(struct device *dev, const char *list_name, const char *cells_name,
                               int cell_count, int index, struct of_phandle_args *out_args)
{
    return __of_parse_phandle_with_args(dev->of_node, list_name, cells_name, cell_count, index,
                                        out_args);
}

int dev_read_u32_array(struct device *dev, const char *propname, uint32_t *out_values, size_t sz)
{
    return of_property_read_u32_array(dev->of_node, propname, out_values, sz);
}

const char *dev_read_string(struct device *dev, const char *propname)
{
    const char *str = NULL;
    int ret = of_property_read_string(dev->of_node, propname, &str);
    if (ret < 0)
    {
        return NULL;
    }
    return str;
}

int dev_property_read_string_index(struct device *dev, const char *propname, int index,
                                   const char **output)
{
    return of_property_read_string_index(dev->of_node, propname, index, output);
}

volatile void *dev_read_addr(struct device *dev)
{
    phys_addr_t reg_addr;
    size_t reg_size;
    int ret;
    ret = of_device_get_resource_reg(dev, &reg_addr, &reg_size);
    if (ret < 0)
    {
        KLOG_E("device get reg error");
        return NULL;
    }
    KLOG_I("reg: %" PRIx64 ", size: 0x%" PRIxPTR, reg_addr, reg_size);

    /* 使用托管 I/O 映射，自动释放 */
    return devm_ioremap(dev, reg_addr, reg_size);
}

bool dev_read_bool(struct device *dev, const char *propname)
{
    return of_property_read_bool(dev->of_node, propname);
}

static void device_release(struct kobject *kobj)
{
    struct device *dev = kobj_to_dev(kobj);
    struct device_private *p = dev->p;

    /*
     * Some platform devices are driven without driver attached
     * and managed resources may have been acquired.  Make sure
     * all resources are released.
     *
     * Drivers still can add resources into device after device
     * is deleted but alive, so release devres here to avoid
     * possible memory leak.
     */
    devres_release_all(dev);

    // kfree(dev->dma_range_map);

    if (dev->release)
        dev->release(dev);
    else
        KLOG_E("Device '%s' does not have a release() function, it is broken and must be fixed. "
               "See Documentation/core-api/kobject.rst.\n",
               dev_name(dev));
    kfree(p);
}

static void device_get_ownership(const struct kobject *kobj, uid_t *uid, gid_t *gid)
{
    const struct device *dev = kobj_to_dev(kobj);
    *uid = 0; /* for root*/
    *gid = 0; /* for root*/
}

#define to_dev_attr(_attr) container_of(_attr, struct device_attribute, attr)

static ssize_t dev_attr_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
    // struct device_attribute *dev_attr = to_dev_attr(attr);
    // struct device *dev = kobj_to_dev(kobj);
    ssize_t ret = -EIO;

    // if (dev_attr->show)
    // 	ret = dev_attr->show(dev, dev_attr, buf);
    // if (ret >= (ssize_t)PAGE_SIZE) {
    // 	printk("dev_attr_show: %pS returned bad count\n",
    // 			dev_attr->show);
    // }
    return ret;
}

static ssize_t dev_attr_store(struct kobject *kobj, struct attribute *attr, const char *buf,
                              size_t count)
{
    // struct device_attribute *dev_attr = to_dev_attr(attr);
    // struct device *dev = kobj_to_dev(kobj);
    ssize_t ret = -EIO;

    // if (dev_attr->store)
    // 	ret = dev_attr->store(dev, dev_attr, buf, count);
    return ret;
}

static const struct sysfs_ops dev_sysfs_ops = {
    .show = dev_attr_show,
    .store = dev_attr_store,
};

static const struct kobj_type device_ktype = {
    .release = device_release,
    .sysfs_ops = &dev_sysfs_ops,
    // .namespace	= device_namespace,
    .get_ownership = device_get_ownership,
};

static int dev_uevent_filter(const struct kobject *kobj)
{
    const struct kobj_type *ktype = get_ktype(kobj);

    if (ktype == &device_ktype)
    {
        const struct device *dev = kobj_to_dev(kobj);
        if (dev->bus)
            return 1;
        if (dev->class)
            return 1;
    }
    return 0;
}

static const char *dev_uevent_name(const struct kobject *kobj)
{
    const struct device *dev = kobj_to_dev(kobj);

    if (dev->bus)
        return dev->bus->name;
    if (dev->class)
        return dev->class->name;
    return NULL;
}

static int dev_uevent(const struct kobject *kobj, struct kobj_uevent_env *env)
{
    const struct device *dev = kobj_to_dev(kobj);
    int retval = 0;
    return retval;
}

static const struct kset_uevent_ops device_uevent_ops = {
    .filter = dev_uevent_filter,
    .name = dev_uevent_name,
    .uevent = dev_uevent,
};

int devices_init(void)
{
    devices_kset = kset_create_and_add("devices", &device_uevent_ops, NULL);
    if (!devices_kset)
        return -ENOMEM;
    dev_kobj = kobject_create_and_add("dev", NULL);
    if (!dev_kobj)
        goto dev_kobj_err;
    sysfs_dev_block_kobj = kobject_create_and_add("block", dev_kobj);
    if (!sysfs_dev_block_kobj)
        goto block_kobj_err;
    sysfs_dev_char_kobj = kobject_create_and_add("char", dev_kobj);
    if (!sysfs_dev_char_kobj)
        goto char_kobj_err;
    // device_link_wq = alloc_workqueue("device_link_wq", 0, 0);
    // if (!device_link_wq)
    // 	goto wq_err;

    return 0;

wq_err:
    kobject_put(sysfs_dev_char_kobj);
char_kobj_err:
    kobject_put(sysfs_dev_block_kobj);
block_kobj_err:
    kobject_put(dev_kobj);
dev_kobj_err:
    kset_unregister(devices_kset);
    return -ENOMEM;
}

int buses_init(void);
int device_system_init(void)
{
    devices_init();
    buses_init();
    return 0;
}
INIT_EXPORT_PRE_DEVSYS(device_system_init, "device system init");
