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

#ifndef __PLATFORM_H__
#define __PLATFORM_H__

#include <driver/acpi.h>
#include <driver/device.h>
#include <driver/driver.h>

#define PLATFORM_NAME_SIZE 20

struct platform_device_id
{
    char name[PLATFORM_NAME_SIZE];
    void *driver_data;
};

struct platform_device
{
    const char *name;
    int id;
    bool id_auto;
    struct device dev;

#ifdef CONFIG_ACPI
    struct acpi_device adev;
#endif
    // u64		platform_dma_mask;
    // struct device_dma_parameters dma_parms;
    // u32		num_resources;
    // struct resource	*resource;

    const struct platform_device_id *id_entry;
    /*
     * Driver name to force a match.  Do not set directly, because core
     * frees it.  Use driver_set_override() to set or clear it.
     */
    const char *driver_override;

    /* MFD cell pointer */
    // struct mfd_cell *mfd_cell;

    /* arch specific additions */
    // struct pdev_archdata	archdata;
};

struct platform_driver
{
    int (*probe)(struct platform_device *);
    void (*remove)(struct platform_device *);
    void (*shutdown)(struct platform_device *);
    // int (*suspend)(struct platform_device *, pm_message_t state);
    // int (*resume)(struct platform_device *);
    struct driver driver;
    const struct platform_device_id *id_table;
    bool prevent_deferred_probe;
    /*
     * For most device drivers, no need to care about this flag as long as
     * all DMAs are handled through the kernel DMA API. For some special
     * ones, for example VFIO drivers, they know how to manage the DMA
     * themselves and set this flag so that the IOMMU layer will allow them
     * to setup and manage their own I/O address space.
     */
    bool driver_managed_dma;
};

#define dev_is_platform(dev) ((dev)->bus == &platform_bus_type)
#define to_platform_device(x) container_of((x), struct platform_device, dev)

#define to_platform_driver(drv) (container_of((drv), struct platform_driver, driver))

int platform_add_driver(struct platform_driver *drv);
int platform_device_register(struct platform_device *pdev);
struct device *platform_bus_find_device_byname(char *name);
#ifdef CONFIG_ACPI
int platform_create_acpi_device(ACPI_HANDLE ObjHandle);
#endif

#endif