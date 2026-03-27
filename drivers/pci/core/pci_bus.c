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
#include <driver/pci/pci_host.h>
#include <errno.h>
#include <fs/fs.h>
#include <stdio.h>
#include <system/macros.h>
#include <ttos_init.h>

#undef KLOG_TAG
#undef KLOG_LEVEL
#define KLOG_TAG "pci-bus"
#define KLOG_LEVEL KLOG_DEBUG
#include <klog.h>

static uint32_t busno = 0;

int pci_register_driver(struct pci_driver *driver)
{
    strcpy(driver->driver.name, driver->name);
    return devbus_add_driver(&pci_bus_type, &driver->driver);
}

static inline int pci_ids_valid(const struct pci_device_id *ids)
{
    return (ids->device | ids->vendor | ids->subdevice | ids->subvendor | ids->class) ? 1 : 0;
}

static int pci_bus_match(struct device *dev, const struct driver *drv)
{
    struct pci_dev *pci_device;
    struct pci_driver *pci_driver;
    const struct pci_device_id *ids;
    int match;

    if (!dev || !drv)
    {
        return 0;
    }

    pci_device = container_of(dev, struct pci_dev, dev);

    if (pci_device->is_bridge)
    {
        return 0;
    }

    pci_driver = container_of(drv, struct pci_driver, driver);
    ids = pci_driver->id_table;
    match = 0;

    for (; pci_ids_valid(ids); ids++)
    {
        if (ids->class && ((pci_device->class & ids->class_mask) == ids->class))
        {
            match = 1;
            break;
        }
        else if ((ids->device != PCI_ANY_ID) && (ids->vendor != PCI_ANY_ID) &&
                 (ids->device == pci_device->device) && (ids->vendor == pci_device->vendor))
        {
            if ((ids->subdevice != PCI_ANY_ID) && (ids->subvendor != PCI_ANY_ID) &&
                (ids->subdevice != pci_device->subsystem_device) &&
                (ids->subvendor != pci_device->subsystem_vendor))
            {
                break;
            }

            match = 1;
            break;
        }
    }

    if (match)
    {
        pci_device->priv = (void *)ids;
        pci_device->driver = pci_driver;
    }

    return match;
}

static int pci_bus_probe(struct device *dev)
{
    struct pci_dev *pci_device;
    struct pci_driver *pci_driver;
    int rc = -1;

    pci_device = container_of(dev, struct pci_dev, dev);
    pci_driver = pci_device->driver;

    if (pci_driver->probe)
    {
        rc = pci_driver->probe(pci_device, (const struct pci_device_id *)pci_device->priv);
    }

    return rc;
}

static void pci_bus_remove(struct device *dev)
{
    /* TODO: Implement remove logic */
}

struct bus_type pci_bus_type = {
    .name = "pci",
    .match = pci_bus_match,
    .probe = pci_bus_probe,
    .remove = pci_bus_remove,
};

int pci_register_device(struct pci_dev *device)
{
    device->dev.bus = &pci_bus_type;
    return register_device(&device->dev);
}

static int pci_bus_init(void)
{
    int ret;

    ret = devbus_register(&pci_bus_type);
    if (ret < 0)
    {
        return -1;
    }

    return 0;
}

INIT_EXPORT_DEV_BUS(pci_bus_init, "pci bus init");

#ifdef CONFIG_SHELL
#include <shell.h>

static int show_pci_device(struct device *dev, void *data)
{
    struct pci_dev *pci_dev = container_of(dev, struct pci_dev, dev);
    printk("%02x:%02x.%x %04x: %04x:%04x", PCI_BUS(pci_dev->bdf), PCI_DEV(pci_dev->bdf),
           PCI_FUN(pci_dev->bdf), PCI_CLASS(pci_dev->class), pci_dev->vendor, pci_dev->device);

    if (PCI_REV(pci_dev->class))
    {
        printk(" (rev %d)", PCI_REV(pci_dev->class));
    }
    printk("\n");
    return 0;
}

static int listpci(void)
{
    /* TODO: Reimplement with new bus architecture */
    /* struct bus_type *bus;

    int bus_no;

    for (bus_no = 0; bus_no < 256; bus_no++)
    {
        bus = devbus_get_bus(PCI_BUS, bus_no);

        if (bus)
        {
            devbus_foreach_dev(bus, show_pci_device, NULL);
        }
    } */
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) |
                     SHELL_CMD_DISABLE_RETURN,
                 lspci, listpci, lspci - n);

#endif