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

#include <acpi.h>
#include <driver/acpi.h>
#include <driver/platform.h>

#define KLOG_TAG "ACPI"
#include <klog.h>

ACPI_RESOURCE *acpi_device_get_resource(struct acpi_device *adev, UINT32 type)
{
    ACPI_RESOURCE *res = adev->res_lst;

    while (res)
    {
        if (res->Type == type)
        {
            return res;
        }
        res = ACPI_NEXT_RESOURCE(res);
    }
    return NULL;
}

ACPI_PCI_ROUTING_TABLE *acpi_device_get_prt(struct acpi_device *adev)
{
    ACPI_BUFFER rt_buffer;
    ACPI_STATUS Status;
    ACPI_PCI_ROUTING_TABLE *Prt;

    rt_buffer.Pointer = NULL;
    rt_buffer.Length = ACPI_ALLOCATE_LOCAL_BUFFER;

    Status = AcpiGetIrqRoutingTable(adev->handle, &rt_buffer);
    if (ACPI_FAILURE(Status))
    {
        return NULL;
    }
    Prt = rt_buffer.Pointer;
    return Prt;
}

bool acpi_device_enabled(struct acpi_device *adev)
{
    ACPI_OBJECT_LIST input;

    ACPI_BUFFER output;
    input.Count = 0; // `_STA` 无需输入参数
    input.Pointer = NULL;
    output.Length = ACPI_ALLOCATE_LOCAL_BUFFER;
    output.Pointer = NULL;

    ACPI_STATUS status = AcpiEvaluateObject(adev->handle, "_STA", &input, &output);
    if (ACPI_FAILURE(status))
    {
        /* 设备没有_STA 方法 表示设备的启用与否不需要查询 */
        return true;
    }
    ACPI_OBJECT *obj = output.Pointer;
    if (obj->Type != ACPI_TYPE_INTEGER)
    {
        KLOG_E("_STA method return value is not an integer");
        ACPI_FREE(output.Pointer);
        return false;
    }
    int value = obj->Integer.Value;
    ACPI_FREE(output.Pointer);
    return value & ACPI_STA_DEVICE_ENABLED;
}
