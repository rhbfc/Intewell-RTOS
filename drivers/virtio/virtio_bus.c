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

#include "virtio.h"
#include <assert.h>
#include <driver/devbus.h>
#include <driver/device.h>
#include <driver/driver.h>
#include <driver/of.h>
#include <errno.h>
#include <ttos.h>

#include <ttos_init.h>

#define KLOG_TAG "virtio-bus"
#include <klog.h>

static int virtio_bus_match(struct device *dev, const struct driver *drv)
{
    struct virtio_device *vdev = to_virtio_device(dev);
    struct virtio_driver *vdrv = to_virtio_driver(drv);
    if (vdev->id.device == vdrv->device)
    {
        return 1;
    }
    return 0;
}

static int virtio_bus_probe(struct device *dev)
{
    int rc;
    struct virtio_device *vdev = to_virtio_device(dev);
    struct virtio_driver *vdrv = to_virtio_driver(dev->driver);

    return vdrv->probe(vdev);
}

static void virtio_bus_remove(struct device *dev)
{
    struct virtio_device *vdev = to_virtio_device(dev);
    struct virtio_driver *vdrv = to_virtio_driver(dev->driver);

    if (vdrv->remove)
    {
        vdrv->remove(vdev);
    }

    return;
}

struct bus_type virtio_bus_type = {
    .name = "virtio",
    .match = virtio_bus_match,
    .probe = virtio_bus_probe,
    .remove = virtio_bus_remove,
};

void *virtio_alloc_buf(struct virtio_device *vdev, size_t size, size_t align)
{
    if (align == 0)
    {
        return malloc(size);
    }
    else
    {
        return memalign(align, size);
    }
}

void *virtio_zalloc_buf(struct virtio_device *vdev, size_t size, size_t align)
{
    void *ptr = virtio_alloc_buf(vdev, size, align);
    if (ptr != NULL)
    {
        memset(ptr, 0, size);
    }

    return ptr;
}

void virtio_free_buf(struct virtio_device *vdev, void *buf)
{
    free(buf);
}

int virtio_add_driver(struct virtio_driver *vdrv)
{
    return devbus_add_driver(&virtio_bus_type, &vdrv->driver);
}

int virtio_device_register(struct virtio_device *vdev)
{
    /* Initialize generic device fields (lists, devres lock, etc.)
     * before any devm_* usage in driver probe paths.
     */
    initialize_device(&vdev->device);
    vdev->device.bus = &virtio_bus_type;
    register_device(&vdev->device);

    return 0;
}

static int virtio_bus_init(void)
{
    return devbus_register(&virtio_bus_type);
}
INIT_EXPORT_DEV_BUS(virtio_bus_init, "virtio bus init");
