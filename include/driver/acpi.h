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

#ifndef __DRV_ACPI_H__
#define __DRV_ACPI_H__

#include <stdint.h>

struct acpi_device_id;

#ifdef CONFIG_ACPI
#include <acpica/source/include/acpi.h>

struct acpi_device
{
    ACPI_HANDLE handle;
    char *path;
    ACPI_RESOURCE *res_lst;
    ACPI_DEVICE_INFO *dev_info;
};

int acpica_init(void);

ACPI_RESOURCE *acpi_device_get_resource(struct acpi_device *adev, UINT32 type);
ACPI_PCI_ROUTING_TABLE *acpi_device_get_prt(struct acpi_device *adev);
bool acpi_device_enabled(struct acpi_device *adev);

#endif

#define ACPI_ID_LEN 16

struct acpi_device_id
{
    uint8_t id[ACPI_ID_LEN];
    void *driver_data;
    uint32_t cls;
    uint32_t cls_msk;
};

#endif
