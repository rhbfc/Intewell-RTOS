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
#include <driver/of.h>
#include <driver/platform.h>
#include <errno.h>
#include <ttosMM.h>
#include <ttos_init.h>
#include <ttos_pic.h>
#ifdef CONFIG_ACPI
#include <accommon.h>
#endif

#define KLOG_TAG "PLATFORM"
#include <klog.h>

static struct device platform_bus = {
    .init_name = "platform",
};

static bool of_driver_match_device(struct device *dev, const struct driver *drv)
{
    const struct of_device_id *match;

    if (!dev || !dev->of_node || !drv || !drv->of_match_table)
    {
        return false;
    }

    if (!of_device_is_available(dev->of_node))
    {
        return false;
    }

    if (dev->parent && (dev->of_node == dev->parent->of_node))
    {
        return false;
    }

    match = of_match_node(drv->of_match_table, dev->of_node);
    if (!match)
    {
        return false;
    }

    dev->driver_data = match->data;

    return true;
}
#ifdef CONFIG_ACPI
static const struct acpi_device_id *acpi_match_node(const struct acpi_device_id *id,
                                                    struct platform_device *pdev)
{
    const struct acpi_device_id *acpi_id = (struct acpi_device_id *)id;

    if (!acpi_device_enabled(&pdev->adev))
    {
        return NULL;
    }

    if (pdev->adev.dev_info->Valid & ACPI_VALID_HID)
    {
        for (; acpi_id->id[0] || acpi_id->cls; acpi_id++)
        {
            if (!strcmp(acpi_id->id, pdev->adev.dev_info->HardwareId.String))
            {
                return acpi_id;
            }
        }
    }

    if (pdev->adev.dev_info->Valid & ACPI_VALID_CID)
    {
        for (acpi_id = id; acpi_id->id[0] || acpi_id->cls; acpi_id++)
        {
            if (!strcmp(acpi_id->id, pdev->adev.dev_info->CompatibleIdList.Ids->String))
            {
                return acpi_id;
            }
        }
    }

    if (pdev->adev.dev_info->Valid & ACPI_VALID_CLS && acpi_id->cls)
    {
        for (acpi_id = id; acpi_id->id[0] || acpi_id->cls; acpi_id++)
        {
            uint32_t cls = strtoul(pdev->adev.dev_info->ClassCode.String, NULL, 16);
            if ((cls & acpi_id->cls_msk) == acpi_id->cls)
            {
                return acpi_id;
            }
        }
    }

    return NULL;
}
#endif
static bool acpi_driver_match_device(struct device *dev, const struct driver *drv)
{
#ifdef CONFIG_ACPI
    struct platform_device *pdev = to_platform_device(dev);
    struct platform_driver *pdrv = to_platform_driver(drv);
    const struct acpi_device_id *match;
    if (pdrv->driver.acpi_match_table)
    {
        if (pdev->adev.handle)
        {
            if (pdev->adev.dev_info)
            {
                match = acpi_match_node(pdrv->driver.acpi_match_table, pdev);
                if (match)
                {
                    dev->driver_data = match->driver_data;
                    return true;
                }
            }
        }
    }
#endif
    return false;
}

static const struct platform_device_id *platform_match_id(const struct platform_device_id *id,
                                                          struct platform_device *pdev)
{
    while (id->name[0])
    {
        if (strcmp(pdev->name, id->name) == 0)
        {
            pdev->id_entry = id;
            return id;
        }
        id++;
    }
    return NULL;
}

static int platform_match(struct device *dev, const struct driver *drv)
{
    struct platform_device *pdev = to_platform_device(dev);
    struct platform_driver *pdrv = to_platform_driver(drv);

    /* 先尝试 of 匹配 */
    if (of_driver_match_device(dev, drv))
    {
        return 1;
    }

    /* 匹配ACPI表 */
    if (acpi_driver_match_device(dev, drv))
    {
        return 1;
    }

    /* 匹配platform_id */
    if (pdrv->id_table)
    {
        return platform_match_id(pdrv->id_table, pdev) != NULL;
    }

    /* 如果都不匹配就直接匹配驱动名和设备名 */
    return (strcmp(pdev->name, drv->name) == 0);
}

static int platform_probe(struct device *dev)
{
    int rc;
    struct platform_driver *pdrv = to_platform_driver(dev->driver);
    struct platform_device *pdev = to_platform_device(dev);

    // todo clock
    if (pdrv->driver.priv_auto_alloc_size)
    {
        dev->priv = calloc(1, pdrv->driver.priv_auto_alloc_size);
    }
    if (pdrv->driver.platdata_auto_alloc_size)
    {
        dev->platdata = calloc(1, pdrv->driver.platdata_auto_alloc_size);
    }

    /* Call probe function with platform_device */
    if (pdrv->probe)
        rc = pdrv->probe(pdev);

    return rc;
}

static void platform_remove(struct device *dev)
{
    struct platform_driver *pdrv = to_platform_driver(dev->driver);
    struct platform_device *pdev = to_platform_device(dev);

    /* 调用驱动的 remove 回调 */
    if (pdrv->remove)
    {
        pdrv->remove(pdev);
    }

    /* 释放自动分配的资源 */
    if (pdrv->driver.priv_auto_alloc_size)
    {
        free(dev->priv);
    }

    if (pdrv->driver.platdata_auto_alloc_size)
    {
        free(dev->platdata);
    }
    return;
}

static void platform_device_release(struct device *dev)
{
    struct platform_device *pdev = to_platform_device(dev);
    of_node_put(dev->of_node);
    dev->of_node = NULL;
#ifdef CONFIG_ACPI
    if (pdev->adev.dev_info)
    {
        ACPI_FREE(pdev->adev.dev_info);
    }
#endif
    free((void *)pdev->name);
    free(pdev);
}

struct bus_type platform_bus_type = {
    .name = "platform",
    // .dev_groups	= platform_dev_groups,
    .match = platform_match,
    // .uevent		= platform_uevent,
    .probe = platform_probe,
    .remove = platform_remove,
    // .shutdown	= platform_shutdown,
    // .dma_configure	= platform_dma_configure,
    // .dma_cleanup	= platform_dma_cleanup,
    // .pm		= &platform_dev_pm_ops,
};

int platform_device_register(struct platform_device *pdev)
{
    initialize_device(&pdev->dev);
    pdev->dev.bus = &platform_bus_type;
    pdev->dev.release = platform_device_release;
    return register_device(&pdev->dev);
}

static int platform_bus_of_probe(struct device_node *node, struct device *parent)
{
    int rc;
    struct platform_device *pdev;
    struct device_node *child;

    if (!node)
    {
        return -EFAULT;
    }

    pdev = calloc(1, sizeof(struct platform_device));
    if (!pdev)
    {
        return -ENOMEM;
    }

    pdev->name = strdup(node->name);
    if (pdev->name == NULL)
    {
        free(pdev);
        return -ENOMEM;
    }

    strncpy(pdev->dev.name, pdev->name, sizeof(pdev->dev.name));
    pdev->dev.of_node = of_node_get(node);
    pdev->dev.of_node->device = (void *)&pdev->dev;
    pdev->dev.parent = parent;
    pdev->dev.priv = NULL;

    rc = platform_device_register(pdev);
    if (rc)
    {
        free((void *)pdev->name);
        free(pdev);
        return rc;
    }
    child = node->child;
    if (child)
    {
        platform_bus_of_probe(child, &pdev->dev);
        for (child = child->sibling; child != NULL; child = child->sibling)
        {
            platform_bus_of_probe(child, &pdev->dev);
        }
    }

    return 0;
}

int platform_device_of_create(void)
{
    struct device_node *node = of_get_root_node();
    /* not spi */
    if (node == NULL)
    {
        return -1;
    }
    return platform_bus_of_probe(node, NULL);
}

struct find_dev_by_name_arg
{
    const char *name;
    struct device *found;
};

static int find_dev_by_name_cb(struct device *dev, void *data)
{
    struct find_dev_by_name_arg *arg = data;

    /* 比较 device 名字是否一致 */
    if (strcmp(dev_name(dev), arg->name) == 0)
    {
        arg->found = dev;

        /* 找到就停止遍历 */
        return 1;
    }

    return 0; /* 继续遍历 */
}

struct device *platform_bus_find_device_byname(char *name)
{
    struct find_dev_by_name_arg arg = {
        .name = name,
        .found = NULL,
    };

    /* 遍历 platform bus */
    devbus_foreach_dev(&platform_bus_type, find_dev_by_name_cb, &arg);

    return arg.found;
}

#ifdef CONFIG_ACPI

int platform_device_acpi_create(void)
{
    acpica_init();
    return 0;
}

#define ACPI_DEVICE_VALID (ACPI_VALID_HID | ACPI_VALID_UID | ACPI_VALID_CID | ACPI_VALID_CLS)

static char *dup_acpi_node_name(ACPI_HANDLE handle)
{
    char NameBuffer[128];
    ACPI_BUFFER buffer = {sizeof(NameBuffer), NameBuffer};

    AcpiGetName(handle, ACPI_FULL_PATHNAME, &buffer);

    return strdup(NameBuffer);
}

int platform_create_acpi_device(ACPI_HANDLE ObjHandle)
{
    ACPI_STATUS Status;
    struct platform_device *pdev;
    ACPI_DEVICE_INFO *DeviceInfo;
    ACPI_BUFFER rt_buffer;
    int rc;
    /* Get device information */
    Status = AcpiGetObjectInfo(ObjHandle, &DeviceInfo);
    if (ACPI_SUCCESS(Status))
    {
        if (!(DeviceInfo->Valid & ACPI_DEVICE_VALID))
        {
            ACPI_FREE(DeviceInfo);
            return 0;
        }
        pdev = calloc(1, sizeof(struct platform_device));
        if (!pdev)
        {
            return -ENOMEM;
        }

        pdev->name = dup_acpi_node_name(ObjHandle);
        strncpy(pdev->dev.name, pdev->name, sizeof(pdev->dev.name));
        if (pdev->name == NULL)
        {
            free(pdev);
            return -ENOMEM;
        }

        pdev->adev.handle = ObjHandle;
        pdev->adev.dev_info = DeviceInfo;
        pdev->adev.path = strdup(pdev->name);

        if (DeviceInfo->Valid & ACPI_VALID_HID)
        {
            const AH_DEVICE_ID *info = AcpiAhMatchHardwareId(DeviceInfo->HardwareId.String);
            KLOG_D("ACPI: %s: %s [%s] %s", pdev->name, DeviceInfo->HardwareId.String,
                   DeviceInfo->UniqueId.String, info ? info->Description : "");
        }

        rt_buffer.Pointer = NULL;
        rt_buffer.Length = ACPI_ALLOCATE_LOCAL_BUFFER;

        Status = AcpiGetCurrentResources(ObjHandle, &rt_buffer);
        if (ACPI_FAILURE(Status))
        {
            KLOG_W("AcpiGetCurrentResources failed: %s", AcpiFormatException(Status));
        }
        else
        {
            pdev->adev.res_lst = rt_buffer.Pointer;
        }

        rc = platform_device_register(pdev);
        if (rc)
        {
            free((void *)pdev->name);
            free(pdev);
            return rc;
        }
    }
    return 0;
}
#endif

int platform_add_driver(struct platform_driver *pdrv)
{
    return devbus_add_driver(&platform_bus_type, &pdrv->driver);
}

int platform_bus_init(void)
{
    int ret = devbus_register(&platform_bus_type);
    if (ret < 0)
    {
        return ret;
    }

    platform_device_of_create();

    return 0;
}
INIT_EXPORT_PRE_PLATFORM(platform_bus_init, "platform bus init");
#ifdef CONFIG_ACPI
INIT_EXPORT_DEV_BUS(platform_device_acpi_create, "platform acpi init");
#endif
