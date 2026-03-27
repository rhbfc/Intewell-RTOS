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
#include <driver/device.h>
#include <driver/driver.h>
#include <errno.h>
#include <kmalloc.h>
#include <spinlock.h>
#include <string.h>
#include <ttos_init.h>

#define KLOG_TAG "Bus"
#include <klog.h>

/* Compatibility macros for memory allocation */
#define kzalloc(size, flags) calloc(1, size)
#define kfree(ptr) free(ptr)

/* Compatibility macros for notifier */
#define BLOCKING_INIT_NOTIFIER_HEAD(head)                                                          \
    do                                                                                             \
    {                                                                                              \
    } while (0)

/* Compatibility macros for debug */
#define pr_debug(fmt, ...) KLOG_D(fmt, ##__VA_ARGS__)

#define to_drv_attr(_attr) container_of(_attr, struct driver_attribute, attr)

static int devbus_pair_device(const struct bus_type *bus, struct device *dev);
static int devbus_pair_drv(const struct bus_type *bus, struct driver *drv);

/* /sys/devices/system */
static struct kset *system_kset;

/* /sys/bus */
static struct kset *bus_kset;

/* /sys/devices/ */
extern struct kset *devices_kset;

static ssize_t bus_uevent_store(const struct bus_type *bus, const char *buf, size_t count)
{
    struct subsys_private *sp = bus_to_subsys(bus);
    // int ret;

    // if (!sp)
    // 	return -EINVAL;

    // ret = kobject_synth_uevent(&sp->subsys.kobj, buf, count);
    // subsys_put(sp);

    // if (ret)
    // 	return ret;
    return count;
}

static struct bus_attribute bus_attr_uevent = __ATTR(uevent, 0200, NULL, bus_uevent_store);

/*
 * sysfs bindings for buses
 */
static ssize_t bus_attr_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
    struct bus_attribute *bus_attr = to_bus_attr(attr);
    struct subsys_private *subsys_priv = to_subsys_private(kobj);
    /* return -EIO for reading a bus attribute without show() */
    ssize_t ret = -EIO;

    if (bus_attr->show)
        ret = bus_attr->show(subsys_priv->bus, buf);
    return ret;
}

static ssize_t bus_attr_store(struct kobject *kobj, struct attribute *attr, const char *buf,
                              size_t count)
{
    struct bus_attribute *bus_attr = to_bus_attr(attr);
    struct subsys_private *subsys_priv = to_subsys_private(kobj);
    /* return -EIO for writing a bus attribute without store() */
    ssize_t ret = -EIO;

    if (bus_attr->store)
        ret = bus_attr->store(subsys_priv->bus, buf, count);
    return ret;
}

static const struct sysfs_ops bus_sysfs_ops = {
    .show = bus_attr_show,
    .store = bus_attr_store,
};

static void bus_release(struct kobject *kobj)
{
    struct subsys_private *priv = to_subsys_private(kobj);

    // lockdep_unregister_key(&priv->lock_key);
    kfree(priv);
}

static const struct kobj_type bus_ktype = {
    .sysfs_ops = &bus_sysfs_ops,
    .release = bus_release,
};

static void klist_devices_get(struct klist_node *n)
{
    struct device_private *dev_prv = to_device_private_bus(n);
    struct device *dev = dev_prv->device;

    get_device(dev);
}

static void klist_devices_put(struct klist_node *n)
{
    struct device_private *dev_prv = to_device_private_bus(n);
    struct device *dev = dev_prv->device;

    put_device(dev);
}

int devbus_register(const struct bus_type *bus)
{
    int retval;
    struct subsys_private *priv;
    struct kobject *bus_kobj;
    struct lock_class_key *key;

    priv = kzalloc(sizeof(struct subsys_private), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    priv->bus = bus;
    /* Store the subsys_private pointer in bus_type for later retrieval */
    ((struct bus_type *)bus)->p = priv;

    BLOCKING_INIT_NOTIFIER_HEAD(&priv->bus_notifier);

    bus_kobj = &priv->subsys.kobj;
    retval = kobject_set_name(bus_kobj, "%s", bus->name);
    if (retval)
        goto out;

    bus_kobj->kset = bus_kset;
    bus_kobj->ktype = &bus_ktype;
    priv->drivers_autoprobe = 1;

    retval = kset_register(&priv->subsys);
    if (retval)
        goto out;

    // retval = bus_create_file(bus, &bus_attr_uevent);
    // if (retval)
    // 	goto bus_uevent_fail;

    priv->devices_kset = kset_create_and_add("devices", NULL, bus_kobj);
    if (!priv->devices_kset)
    {
        retval = -ENOMEM;
        goto bus_devices_fail;
    }

    priv->drivers_kset = kset_create_and_add("drivers", NULL, bus_kobj);
    if (!priv->drivers_kset)
    {
        retval = -ENOMEM;
        goto bus_drivers_fail;
    }

    INIT_LIST_HEAD(&priv->interfaces);
    // key = &priv->lock_key;
    // lockdep_register_key(key);
    // __mutex_init(&priv->mutex, "subsys mutex", key);
    klist_init(&priv->klist_devices, klist_devices_get, klist_devices_put);
    klist_init(&priv->klist_drivers, NULL, NULL);

    // retval = add_probe_files(bus);
    // if (retval)
    // 	goto bus_probe_files_fail;

    // retval = sysfs_create_groups(bus_kobj, bus->bus_groups);
    // if (retval)
    // 	goto bus_groups_fail;

    KLOG_D("bus: '%s': registered", bus->name);
    return 0;

bus_groups_fail:
    // remove_probe_files(bus);
bus_probe_files_fail:
    kset_unregister(priv->drivers_kset);
bus_drivers_fail:
    kset_unregister(priv->devices_kset);
bus_devices_fail:
    // bus_remove_file(bus, &bus_attr_uevent);
bus_uevent_fail:
    kset_unregister(&priv->subsys);
    /* Above kset_unregister() will kfree @priv */
    priv = NULL;
out:
    kfree(priv);
    return retval;
}

int devbus_unregister(const struct bus_type *bus)
{
    struct subsys_private *sp = bus_to_subsys(bus);
    // struct kobject *bus_kobj;

    if (!sp)
        return -EINVAL;

    KLOG_D("bus: '%s': unregistering\n", bus->name);
    if (sp->dev_root)
        /* TODO: Implement device_unregister */
        // device_unregister(sp->dev_root);
        ;

    // bus_kobj = &sp->subsys.kobj;
    // sysfs_remove_groups(bus_kobj, bus->bus_groups);
    // remove_probe_files(bus);
    // bus_remove_file(bus, &bus_attr_uevent);

    kset_unregister(sp->drivers_kset);
    kset_unregister(sp->devices_kset);
    kset_unregister(&sp->subsys);
    subsys_put(sp);
    return 0;
}

int devbus_register_device(struct device *dev)
{
    struct subsys_private *sp = bus_to_subsys(dev->bus);
    int error;

    if (!sp)
    {
        /*
         * This is a normal operation for many devices that do not
         * have a bus assigned to them, just say that all went
         * well.
         */
        return 0;
    }

    /*
     * Reference in sp is now incremented and will be dropped when
     * the device is removed from the bus
     */

    // KLOG_D("bus: '%s': add device %s", sp->bus->name, dev_name(dev));

    // error = device_add_groups(dev, sp->bus->dev_groups);
    // if (error)
    // 	goto out_put;

    // error = sysfs_create_link(&sp->devices_kset->kobj, &dev->kobj, dev_name(dev));
    // if (error)
    // 	goto out_groups;

    // error = sysfs_create_link(&dev->kobj, &sp->subsys.kobj, "subsystem");
    // if (error)
    // 	goto out_subsys;

    klist_add_tail(&dev->p->knode_bus, &sp->klist_devices);

    devbus_pair_device(dev->bus, dev);
    return 0;

out_subsys:
    // sysfs_remove_link(&sp->devices_kset->kobj, dev_name(dev));
out_groups:
    // device_remove_groups(dev, sp->bus->dev_groups);
out_put:
    subsys_put(sp);
    return error;
}

int devbus_match_driver(struct device *dev, struct driver *drv)
{
    if (dev && dev->bus && dev->bus->match)
        return dev->bus->match(dev, drv);
    return 0;
}

int devbus_probe_device(struct device *dev)
{
    if (is_device_drivered(dev))
    {
        return dev->bus->probe(dev);
    }
    return -ENOENT;
}

static struct device *next_device(struct klist_iter *i)
{
    struct klist_node *n = klist_next(i);
    struct device *dev = NULL;
    struct device_private *dev_prv;

    if (n)
    {
        dev_prv = to_device_private_bus(n);
        dev = dev_prv->device;
    }
    return dev;
}

struct subsys_private *bus_to_subsys(const struct bus_type *bus)
{
    struct subsys_private *sp = NULL;
    struct kobject *kobj;

    if (!bus || !bus_kset)
        return NULL;

    spin_lock(&bus_kset->list_lock);

    if (list_empty(&bus_kset->list))
        goto done;

    list_for_each_entry(kobj, &bus_kset->list, entry)
    {
        struct kset *kset = container_of(kobj, struct kset, kobj);

        sp = container_of(kset, struct subsys_private, subsys);
        if (sp->bus == bus)
            goto done;
    }
    sp = NULL;
done:
    sp = subsys_get(sp);
    spin_unlock(&bus_kset->list_lock);
    return sp;
}

int devbus_foreach_dev(const struct bus_type *bus, int (*func)(struct device *dev, void *data),
                       void *data)
{
    struct subsys_private *sp = bus_to_subsys(bus);
    struct klist_iter i;
    struct device *dev;
    int error = 0;

    if (!sp)
        return -EINVAL;

    klist_iter_init_node(&sp->klist_devices, &i, NULL);
    while ((dev = next_device(&i)))
    {
        if (0 != func(dev, data))
        {
            break;
        }
    }

    klist_iter_exit(&i);
    subsys_put(sp);
    return error;
}

static struct driver *next_driver(struct klist_iter *i)
{
    struct klist_node *n = klist_next(i);
    struct driver_private *drv_priv;

    if (n)
    {
        drv_priv = container_of(n, struct driver_private, knode_bus);
        return drv_priv->driver;
    }
    return NULL;
}

int devbus_foreach_driver(const struct bus_type *bus, int (*func)(struct driver *drv, void *data),
                          void *data)
{
    struct subsys_private *sp = bus_to_subsys(bus);
    struct klist_iter i;
    struct driver *drv;
    int error = 0;

    if (!sp)
        return -EINVAL;

    klist_iter_init_node(&sp->klist_drivers, &i, NULL);
    while ((drv = next_driver(&i)))
    {
        if (0 != func(drv, data))
        {
            break;
        }
    }

    klist_iter_exit(&i);
    subsys_put(sp);
    return error;
}

static int _devbus_pair_drv(struct device *dev, void *data)
{
    struct driver *drv = (struct driver *)data;
    long flags;

    if (is_device_drivered(dev))
    {
        return 0;
    }

    if (devbus_match_driver(dev, drv))
    {
        dev->driver = drv;
        devbus_probe_device(dev);
    }
    return 0;
}

static int devbus_pair_drv(const struct bus_type *bus, struct driver *drv)
{
    devbus_foreach_dev(bus, _devbus_pair_drv, drv);
    return 0;
}

static int _devbus_pair_device(struct driver *drv, void *data)
{
    struct device *dev = (struct device *)data;
    long flags;

    if (is_device_drivered(dev))
    {
        return 0;
    }

    if (devbus_match_driver(dev, drv))
    {
        dev->driver = drv;
        devbus_probe_device(dev);
    }
    return 0;
}

static int devbus_pair_device(const struct bus_type *bus, struct device *dev)
{
    devbus_foreach_driver(bus, _devbus_pair_device, dev);
    return 0;
}

static ssize_t drv_attr_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
    struct driver_attribute *drv_attr = to_drv_attr(attr);
    struct driver_private *drv_priv = to_driver(kobj);
    ssize_t ret = -EIO;

    if (drv_attr->show)
        ret = drv_attr->show(drv_priv->driver, buf);
    return ret;
}

static ssize_t drv_attr_store(struct kobject *kobj, struct attribute *attr, const char *buf,
                              size_t count)
{
    struct driver_attribute *drv_attr = to_drv_attr(attr);
    struct driver_private *drv_priv = to_driver(kobj);
    ssize_t ret = -EIO;

    if (drv_attr->store)
        ret = drv_attr->store(drv_priv->driver, buf, count);
    return ret;
}

static const struct sysfs_ops driver_sysfs_ops = {
    .show = drv_attr_show,
    .store = drv_attr_store,
};

static void driver_release(struct kobject *kobj)
{
    struct driver_private *drv_priv = to_driver(kobj);

    pr_debug("driver: '%s': %s\n", kobject_name(kobj), __func__);
    kfree(drv_priv);
}

static const struct kobj_type driver_ktype = {
    .sysfs_ops = &driver_sysfs_ops,
    .release = driver_release,
};

int devbus_add_driver(const struct bus_type *bus, struct driver *drv)
{
    struct subsys_private *sp = bus_to_subsys(bus);
    struct driver_private *priv;
    int error = 0;

    if (!sp)
        return -EINVAL;

    /*
     * Reference in sp is now incremented and will be dropped when
     * the driver is removed from the bus
     */
    KLOG_D("bus: '%s': add driver %s", sp->bus->name, drv->name);

    priv = kzalloc(sizeof(*priv), GFP_KERNEL);
    if (!priv)
    {
        error = -ENOMEM;
        goto out_put_bus;
    }
    klist_init(&priv->klist_devices, NULL, NULL);
    priv->driver = drv;
    drv->p = priv;
    priv->kobj.kset = sp->drivers_kset;
    error = kobject_init_and_add(&priv->kobj, &driver_ktype, NULL, "%s", drv->name);
    if (error)
        goto out_unregister;

    klist_add_tail(&priv->knode_bus, &sp->klist_drivers);
    // if (sp->drivers_autoprobe) {
    // 	error = driver_attach(drv);
    // 	if (error)
    // 		goto out_del_list;
    // }
    // error = module_add_driver(drv->owner, drv);
    // if (error) {
    // 	KLOG_E("%s: failed to create module links for %s",
    // 		__func__, drv->name);
    // 	goto out_detach;
    // }

    // error = driver_create_file(drv, &driver_attr_uevent);
    // if (error) {
    // 	KLOG_E("%s: uevent attr (%s) failed\n",
    // 		__func__, drv->name);
    // }
    // error = driver_add_groups(drv, sp->bus->drv_groups);
    // if (error) {
    // 	/* How the hell do we get out of this pickle? Give up */
    // 	KLOG_E("%s: driver_add_groups(%s) failed",
    // 		__func__, drv->name);
    // }

    // if (!drv->suppress_bind_attrs) {
    // 	error = add_bind_files(drv);
    // 	if (error) {
    // 		/* Ditto */
    // 		KLOG_E("%s: add_bind_files(%s) failed",
    // 			__func__, drv->name);
    // 	}
    // }

    devbus_pair_drv(bus, drv);

    return 0;

out_detach:
    // driver_detach(drv);
out_del_list:
    klist_del(&priv->knode_bus);
out_unregister:
    kobject_put(&priv->kobj);
    /* drv->p is freed in driver_release()  */
    drv->p = NULL;
out_put_bus:
    subsys_put(sp);
    return error;
}

static int _devbus_match_name(struct device *dev, void *data)
{
    struct
    {
        const char *name;
        struct device **pdev;
    } *ctx = data;
    if (strcmp(dev->name, ctx->name) == 0)
    {
        *ctx->pdev = dev;
        return 1;
    }
    return 0;
}

struct device *devbus_find_device_byname(char *name)
{
    /* TODO: Reimplement using kobject/kset infrastructure */
    /* This function needs to iterate through all registered buses and devices */
    /* using the new kobject-based architecture instead of the old bus_list */
    KLOG_W("devbus_find_device_byname: not yet implemented with kobject architecture");
    return NULL;
}

struct bus_type *devbus_get_bus(uint32_t type_index, uint32_t bus_id)
{
    /* TODO: Reimplement using kobject/kset infrastructure */
    /* This function needs to iterate through all registered buses */
    /* using the new kobject-based architecture instead of the old bus_list */
    KLOG_W("devbus_get_bus: not yet implemented with kobject architecture");
    return NULL;
}

static int bus_uevent_filter(const struct kobject *kobj)
{
    const struct kobj_type *ktype = get_ktype(kobj);

    if (ktype == &bus_ktype)
        return 1;
    return 0;
}

static const struct kset_uevent_ops bus_uevent_ops = {
    .filter = bus_uevent_filter,
};

int buses_init(void)
{
    bus_kset = kset_create_and_add("bus", &bus_uevent_ops, NULL);
    if (!bus_kset)
        return -ENOMEM;

    system_kset = kset_create_and_add("system", NULL, &devices_kset->kobj);
    if (!system_kset)
    {
        /* Do error handling here as devices_init() do */
        kset_unregister(bus_kset);
        bus_kset = NULL;
        KLOG_E("%s: failed to create and add kset 'bus'", __func__);
        return -ENOMEM;
    }

    return 0;
}