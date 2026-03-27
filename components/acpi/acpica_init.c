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

#include "acpi.h"

#include "accommon.h"

#include <driver/platform.h>
#include <multiboot.h>
#include <stdio.h>
#include <string.h>
#include <ttos_init.h>

#include <asm/io.h>

#define KLOG_TAG "ACPICA_INIT"
#include <klog.h>

#define _COMPONENT ACPI_EXAMPLE
ACPI_MODULE_NAME("init")

//#define ACPICA_DUMP_INFO

/******************************************************************************
 *
 * ACPICA Example Code
 *
 * This module contains examples of how the host OS should interface to the
 * ACPICA subsystem.
 *
 * 1) How to use the platform/acenv.h file and how to set configuration
 *      options.
 *
 * 2) main - using the debug output mechanism and the error/warning output
 *      macros.
 *
 * 3) Two examples of the ACPICA initialization sequence. The first is a
 *      initialization with no "early" ACPI table access. The second shows
 *      how to use ACPICA to obtain the tables very early during kernel
 *      initialization, even before dynamic memory is available.
 *
 * 4) How to invoke a control method, including argument setup and how to
 *      access the return value.
 *
 *****************************************************************************/

/* Local Prototypes */

static ACPI_STATUS InitializeFullAcpica(void);

static ACPI_STATUS InstallHandlers(void);

static void NotifyHandler(ACPI_HANDLE Device, UINT32 Value, void *Context);

static ACPI_STATUS DeviceCallback(ACPI_HANDLE ObjHandle, UINT32 NestingLevel, void *Context,
                                  void **ReturnValue);

#ifdef ACPICA_DUMP_INFO

/******************************************************************************
 *
 * FUNCTION:    DumpTableInfo
 *
 * PARAMETERS:  Table - Pointer to ACPI table header
 *
 * RETURN:      None
 *
 * DESCRIPTION: Dump information about an ACPI table
 *
 *****************************************************************************/

static void DumpTableInfo(ACPI_TABLE_HEADER *Table)
{
    if (!Table)
    {
        return;
    }

    KLOG_I("  Signature: %.4s", Table->Signature);
    KLOG_I("  Length: %u bytes", Table->Length);
    KLOG_I("  Revision: %u", Table->Revision);
    KLOG_I("  Checksum: 0x%02x", Table->Checksum);
    KLOG_I("  OEM ID: %.6s", Table->OemId);
    KLOG_I("  OEM Table ID: %.8s", Table->OemTableId);
    KLOG_I("  OEM Revision: 0x%08x", Table->OemRevision);
}

/******************************************************************************
 *
 * FUNCTION:    DumpFadtTable
 *
 * PARAMETERS:  Table - Pointer to FADT table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Dump detailed FADT table information
 *
 *****************************************************************************/

static void DumpFadtTable(ACPI_TABLE_FADT *Fadt)
{
    if (!Fadt)
    {
        return;
    }

    KLOG_I("[FADT] Firmware ACPI Control Structure");
    DumpTableInfo(&Fadt->Header);

    KLOG_I("  FACS Address (32-bit): 0x%08x", Fadt->Facs);
    KLOG_I("  DSDT Address (32-bit): 0x%08x", Fadt->Dsdt);
    KLOG_I("  Preferred PM Profile: %u", Fadt->PreferredProfile);
    KLOG_I("  SCI Interrupt: %u", Fadt->SciInterrupt);
    KLOG_I("  SMI Command Port: 0x%08x", Fadt->SmiCommand);
    KLOG_I("  ACPI Enable Value: 0x%02x", Fadt->AcpiEnable);
    KLOG_I("  ACPI Disable Value: 0x%02x", Fadt->AcpiDisable);
    KLOG_I("  S4BIOS Command: 0x%02x", Fadt->S4BiosRequest);
    KLOG_I("  PM1A Event Block Address: 0x%08x (length: %u)", Fadt->Pm1aEventBlock,
           Fadt->Pm1EventLength);
    KLOG_I("  PM1B Event Block Address: 0x%08x (length: %u)", Fadt->Pm1bEventBlock,
           Fadt->Pm1EventLength);
    KLOG_I("  PM1A Control Block Address: 0x%08x (length: %u)", Fadt->Pm1aControlBlock,
           Fadt->Pm1ControlLength);
    KLOG_I("  PM1B Control Block Address: 0x%08x (length: %u)", Fadt->Pm1bControlBlock,
           Fadt->Pm1ControlLength);
    KLOG_I("  PM2 Control Block Address: 0x%08x (length: %u)", Fadt->Pm2ControlBlock,
           Fadt->Pm2ControlLength);
    KLOG_I("  PM Timer Block Address: 0x%08x (length: %u)", Fadt->PmTimerBlock,
           Fadt->PmTimerLength);
    KLOG_I("  GPE0 Block Address: 0x%08x (length: %u)", Fadt->Gpe0Block, Fadt->Gpe0BlockLength);
    KLOG_I("  GPE1 Block Address: 0x%08x (length: %u)", Fadt->Gpe1Block, Fadt->Gpe1BlockLength);
    KLOG_I("  GPE1 Base: %u", Fadt->Gpe1Base);
    KLOG_I("  C2 Latency: %u us", Fadt->C2Latency);
    KLOG_I("  C3 Latency: %u us", Fadt->C3Latency);
    KLOG_I("  CPU Cache Size: %u KB", Fadt->FlushSize);
    KLOG_I("  Flush Stride: %u", Fadt->FlushStride);
    KLOG_I("  Duty Cycle Offset: %u", Fadt->DutyOffset);
    KLOG_I("  Duty Cycle Width: %u", Fadt->DutyWidth);
    KLOG_I("  RTC Alarm Day Alarm: %u", Fadt->DayAlarm);
    KLOG_I("  RTC Alarm Month Alarm: %u", Fadt->MonthAlarm);
    KLOG_I("  RTC Century: %u", Fadt->Century);
    KLOG_I("  Boot Architecture Flags: 0x%04x", Fadt->BootFlags);
    KLOG_I("  FACS Address (64-bit): 0x%016llx", Fadt->XFacs);
    KLOG_I("  DSDT Address (64-bit): 0x%016llx", Fadt->XDsdt);
}

/******************************************************************************
 *
 * FUNCTION:    ListAllTables
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: List all ACPI tables
 *
 *****************************************************************************/

/******************************************************************************
 *
 * FUNCTION:    DumpApicTable
 *
 * PARAMETERS:  Table - Pointer to APIC (MADT) table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Dump detailed APIC table information including all subtables
 *
 *****************************************************************************/

static void DumpApicTable(ACPI_TABLE_MADT *Madt)
{
    ACPI_SUBTABLE_HEADER *Subtable;
    ACPI_MADT_LOCAL_APIC *LocalApic;
    ACPI_MADT_IO_APIC *IoApic;
    ACPI_MADT_INTERRUPT_OVERRIDE *IntOverride;
    UINT8 *TableEnd;
    UINT32 Offset;

    if (!Madt)
    {
        return;
    }

    KLOG_I("[MADT] Multiple APIC Description Table");
    DumpTableInfo(&Madt->Header);
    KLOG_I("  Local APIC Address: 0x%08x", Madt->Address);
    KLOG_I("  Flags: 0x%08x (PC-AT Compat: %s)", Madt->Flags,
           (Madt->Flags & ACPI_MADT_PCAT_COMPAT) ? "Yes" : "No");

    /* Parse subtables */
    TableEnd = (UINT8 *)Madt + Madt->Header.Length;
    Subtable = (ACPI_SUBTABLE_HEADER *)((UINT8 *)Madt + sizeof(ACPI_TABLE_MADT));

    while ((UINT8 *)Subtable < TableEnd)
    {
        switch (Subtable->Type)
        {
        case ACPI_MADT_TYPE_LOCAL_APIC:
            LocalApic = (ACPI_MADT_LOCAL_APIC *)Subtable;
            KLOG_I("  [LOCAL APIC] ACPI ID: %u, APIC ID: %u, Flags: 0x%02x (Enabled: %s)",
                   LocalApic->ProcessorId, LocalApic->Id, LocalApic->LapicFlags,
                   (LocalApic->LapicFlags & ACPI_MADT_ENABLED) ? "Yes" : "No");
            break;

        case ACPI_MADT_TYPE_IO_APIC:
            IoApic = (ACPI_MADT_IO_APIC *)Subtable;
            KLOG_I("  [IO APIC] ID: %u, Address: 0x%08x, GSI Base: %u", IoApic->Id, IoApic->Address,
                   IoApic->GlobalIrqBase);
            break;

        case ACPI_MADT_TYPE_INTERRUPT_OVERRIDE:
            IntOverride = (ACPI_MADT_INTERRUPT_OVERRIDE *)Subtable;
            KLOG_I("  [INTERRUPT OVERRIDE] Bus: %u, Source: %u, GSI: %u, Flags: 0x%04x",
                   IntOverride->Bus, IntOverride->SourceIrq, IntOverride->GlobalIrq,
                   IntOverride->IntiFlags);
            break;

        default:
            KLOG_I("  [SUBTABLE] Type: %u, Length: %u", Subtable->Type, Subtable->Length);
            break;
        }

        Subtable = (ACPI_SUBTABLE_HEADER *)((UINT8 *)Subtable + Subtable->Length);
    }
}

/******************************************************************************
 *
 * FUNCTION:    DumpMcfgTable
 *
 * PARAMETERS:  Table - Pointer to MCFG table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Dump detailed MCFG table information
 *
 *****************************************************************************/

static void DumpMcfgTable(ACPI_TABLE_MCFG *Mcfg)
{
    ACPI_MCFG_ALLOCATION *Allocation;
    UINT8 *TableEnd;
    UINT32 Count = 0;

    if (!Mcfg)
    {
        return;
    }

    KLOG_I("[MCFG] PCI Express Memory-mapped Configuration Table");
    DumpTableInfo(&Mcfg->Header);

    /* Parse MCFG allocations */
    TableEnd = (UINT8 *)Mcfg + Mcfg->Header.Length;
    Allocation = (ACPI_MCFG_ALLOCATION *)((UINT8 *)Mcfg + sizeof(ACPI_TABLE_MCFG));

    while ((UINT8 *)Allocation < TableEnd)
    {
        KLOG_I("  [ALLOCATION %u]", Count);
        KLOG_I("    Base Address: 0x%016llx", Allocation->Address);
        KLOG_I("    PCI Segment: %u", Allocation->PciSegment);
        KLOG_I("    Start Bus: %u", Allocation->StartBusNumber);
        KLOG_I("    End Bus: %u", Allocation->EndBusNumber);

        Allocation++;
        Count++;
    }

    KLOG_I("  Total MCFG Allocations: %u", Count);
}

static void DumpDeviceInfo(UINT32 NestingLevel, ACPI_DEVICE_INFO *DeviceInfo)
{
    char Indent[128];
    UINT32 i;
    char NameBuffer[5];
    NameBuffer[0] = (char)((DeviceInfo->Name >> 0) & 0xFF);
    NameBuffer[1] = (char)((DeviceInfo->Name >> 8) & 0xFF);
    NameBuffer[2] = (char)((DeviceInfo->Name >> 16) & 0xFF);
    NameBuffer[3] = (char)((DeviceInfo->Name >> 24) & 0xFF);
    NameBuffer[4] = '\0';

    /* Create indentation based on nesting level */
    for (i = 0; i < NestingLevel && i < sizeof(Indent) - 1; i++)
    {
        Indent[i] = ' ';
    }
    Indent[i] = '\0';

    /* Print Device, Processor, and Scope objects */
    if (DeviceInfo->Type == ACPI_TYPE_DEVICE || DeviceInfo->Type == ACPI_TYPE_PROCESSOR)
    {
        KLOG_I("%s[%s] Name: %s", Indent,
               DeviceInfo->Type == ACPI_TYPE_DEVICE ? "Device" : "Processor", NameBuffer);

        /* Print device flags */
        if (DeviceInfo->Flags & ACPI_PCI_ROOT_BRIDGE)
        {
            KLOG_I("%s  -> PCI Root Bridge", Indent);
        }

        /* Print valid fields */
        if (DeviceInfo->Valid & ACPI_VALID_HID)
        {
            KLOG_I("%s  -> Hardware ID: %s", Indent, DeviceInfo->HardwareId.String);
        }
        if (DeviceInfo->Valid & ACPI_VALID_UID)
        {
            KLOG_I("%s  -> Unique ID: %s", Indent, DeviceInfo->UniqueId.String);
        }
        if (DeviceInfo->Valid & ACPI_VALID_ADR)
        {
            KLOG_I("%s  -> Address: 0x%016llx", Indent, DeviceInfo->Address);
        }

        if (DeviceInfo->Valid & ACPI_VALID_CID)
        {
            KLOG_I("%s  -> Compatible IDs:", Indent);
            for (i = 0; i < DeviceInfo->CompatibleIdList.Count; i++)
            {
                KLOG_I("%s    - %s", Indent, DeviceInfo->CompatibleIdList.Ids[i].String);
            }
        }

        if (DeviceInfo->Valid & ACPI_VALID_CLS)
        {
            KLOG_I("%s  -> Class Code: 0x%08x", Indent, DeviceInfo->ClassCode);
        }
    }
}

/******************************************************************************
 *
 * FUNCTION:    DumpHpetTable
 *
 * PARAMETERS:  Table - Pointer to HPET table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Dump detailed HPET table information
 *
 *****************************************************************************/

static void DumpHpetTable(ACPI_TABLE_HEADER *Table)
{
    ACPI_TABLE_HPET *Hpet = (ACPI_TABLE_HPET *)Table;

    if (!Hpet)
    {
        return;
    }

    KLOG_I("[HPET] High Precision Event Timer Table");
    DumpTableInfo(Table);
    KLOG_I("  Hardware ID: 0x%08x", Hpet->Id);
    KLOG_I("  Sequence Number: %u", Hpet->Sequence);
    KLOG_I("  Minimum Tick: %u", Hpet->MinimumTick);
    KLOG_I("  Flags: 0x%02x", Hpet->Flags);
    KLOG_I("  Page Protection: %s",
           (Hpet->Flags & ACPI_HPET_PAGE_PROTECT_MASK) == ACPI_HPET_PAGE_PROTECT4    ? "4K"
           : (Hpet->Flags & ACPI_HPET_PAGE_PROTECT_MASK) == ACPI_HPET_PAGE_PROTECT64 ? "64K"
                                                                                     : "None");
}

/******************************************************************************
 *
 * FUNCTION:    DumpSratTable
 *
 * PARAMETERS:  Table - Pointer to SRAT table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Dump detailed SRAT table information
 *
 *****************************************************************************/

static void DumpSratTable(ACPI_TABLE_HEADER *Table)
{
    ACPI_TABLE_SRAT *Srat = (ACPI_TABLE_SRAT *)Table;
    ACPI_SUBTABLE_HEADER *Subtable;
    ACPI_SRAT_CPU_AFFINITY *CpuAffinity;
    ACPI_SRAT_MEM_AFFINITY *MemAffinity;
    UINT8 *TableEnd;

    if (!Srat)
    {
        return;
    }

    KLOG_I("[SRAT] System Resource Affinity Table");
    DumpTableInfo(Table);
    KLOG_I("  Reserved: 0x%08x", Srat->Reserved);

    /* Parse SRAT subtables */
    TableEnd = (UINT8 *)Srat + Srat->Header.Length;
    Subtable = (ACPI_SUBTABLE_HEADER *)((UINT8 *)Srat + sizeof(ACPI_TABLE_SRAT));

    while ((UINT8 *)Subtable < TableEnd)
    {
        switch (Subtable->Type)
        {
        case ACPI_SRAT_TYPE_CPU_AFFINITY:
            CpuAffinity = (ACPI_SRAT_CPU_AFFINITY *)Subtable;
            KLOG_I("  [CPU AFFINITY] Proximity Domain: %u, APIC ID: %u, Flags: 0x%02x",
                   CpuAffinity->ProximityDomainLo, CpuAffinity->ApicId, CpuAffinity->Flags);
            break;

        case ACPI_SRAT_TYPE_MEMORY_AFFINITY:
            MemAffinity = (ACPI_SRAT_MEM_AFFINITY *)Subtable;
            KLOG_I("  [MEMORY AFFINITY] Proximity Domain: %u, Base: 0x%016llx, Length: 0x%016llx, "
                   "Flags: 0x%02x",
                   MemAffinity->ProximityDomain, MemAffinity->BaseAddress, MemAffinity->Length,
                   MemAffinity->Flags);
            break;

        default:
            KLOG_I("  [SUBTABLE] Type: %u, Length: %u", Subtable->Type, Subtable->Length);
            break;
        }

        Subtable = (ACPI_SUBTABLE_HEADER *)((UINT8 *)Subtable + Subtable->Length);
    }
}

static void ListAllTables(void)
{
    ACPI_TABLE_HEADER *Table;
    ACPI_STATUS Status;
    UINT32 i = 0;

    KLOG_I("========== All ACPI Tables ==========");

    for (i = 0;; i++)
    {
        Status = AcpiGetTableByIndex(i, &Table);
        if (ACPI_FAILURE(Status))
        {
            break;
        }

        KLOG_I("  [%u] %.4s at %p (length: %u bytes)", i, Table->Signature, Table, Table->Length);
    }

    KLOG_I("Total tables: %u", i);
    KLOG_I("========== End of Table List ==========");
}
#endif
/******************************************************************************
 *
 * FUNCTION:    DeviceCallback
 *
 * PARAMETERS:  ObjHandle - Handle to the current object
 *              NestingLevel - Depth in the namespace tree
 *              Context - Context pointer (not used)
 *              ReturnValue - Return value (not used)
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Callback function for walking DSDT device tree
 *
 *****************************************************************************/

static ACPI_STATUS DeviceCallback(ACPI_HANDLE ObjHandle, UINT32 NestingLevel, void *Context,
                                  void **ReturnValue)
{
    /* Limit nesting depth to avoid infinite loops */
    if (NestingLevel > 16)
    {
        return (AE_CTRL_DEPTH);
    }

    platform_create_acpi_device(ObjHandle);

#ifdef ACPICA_DUMP_INFO
    ACPI_DEVICE_INFO *DeviceInfo;
    ACPI_STATUS Status;
    /* Get device information */
    Status = AcpiGetObjectInfo(ObjHandle, &DeviceInfo);
    if (ACPI_SUCCESS(Status))
    {
        DumpDeviceInfo(NestingLevel, DeviceInfo);

        ACPI_FREE(DeviceInfo);
    }
#endif
    return (AE_OK);
}

/******************************************************************************
 *
 * FUNCTION:    EnumerateDevices
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: EnumerateDevices all devices in ROOT namespace
 *
 *****************************************************************************/

static void EnumerateDevices(void)
{
    ACPI_STATUS Status;
#ifdef ACPICA_DUMP_INFO
    KLOG_I("[DSDT] Differential System Description Table - Device Tree");
    KLOG_I("========== DSDT Devices ==========");
#endif
    /* Walk the namespace starting from root */
    Status = AcpiWalkNamespace(ACPI_TYPE_DEVICE, ACPI_ROOT_OBJECT, ACPI_UINT32_MAX, DeviceCallback,
                               NULL, NULL, NULL);

    if (ACPI_FAILURE(Status))
    {
        KLOG_W("Failed to walk ROOT namespace: 0x%x", Status);
    }
#ifdef ACPICA_DUMP_INFO
    KLOG_I("========== End of DSDT Devices ==========");
#endif
}

/******************************************************************************
 *
 * Example ACPICA initialization code. This shows a full initialization with
 * no early ACPI table access.
 *
 *****************************************************************************/

static ACPI_STATUS InitializeFullAcpica(void)
{
    ACPI_STATUS Status;

    /* Initialize the ACPICA subsystem */
    Status = AcpiInitializeSubsystem();
    if (ACPI_FAILURE(Status))
    {
        KLOG_E("Failed to initialize ACPICA subsystem: 0x%x", Status);
        return (Status);
    }

    /* Initialize the ACPICA Table Manager and get all ACPI tables */
    Status = AcpiInitializeTables(NULL, 16, FALSE);
    if (ACPI_FAILURE(Status))
    {
        KLOG_E("Failed to initialize table manager: 0x%x", Status);
        return (Status);
    }

    Status = AcpiLoadTables();
    if (ACPI_FAILURE(Status))
    {
        KLOG_E("Failed to load ACPI tables: 0x%x", Status);
        return (Status);
    }

    /* Install local handlers */
    Status = InstallHandlers();
    if (ACPI_FAILURE(Status))
    {
        KLOG_E("Failed to install handlers: 0x%x", Status);
        return (Status);
    }

    /* Initialize the ACPI hardware */
    Status = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);
    if (ACPI_FAILURE(Status))
    {
        KLOG_E("Failed to enable ACPICA subsystem: 0x%x", Status);
        return (Status);
    }

    /* Complete the ACPI namespace object initialization */
    Status = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION);
    if (ACPI_FAILURE(Status))
    {
        KLOG_E("Failed to initialize ACPI objects: 0x%x", Status);
        return (Status);
    }

    return (AE_OK);
}

/******************************************************************************
 *
 * Example ACPICA handler and handler installation
 *
 *****************************************************************************/

static void NotifyHandler(ACPI_HANDLE Device, UINT32 Value, void *Context)
{
    ACPI_INFO(("Received a notify 0x%X", Value));
}

static ACPI_STATUS InstallHandlers(void)
{
    ACPI_STATUS Status;

    /* Install global notify handler */

    Status = AcpiInstallNotifyHandler(ACPI_ROOT_OBJECT, ACPI_SYSTEM_NOTIFY, NotifyHandler, NULL);
    if (ACPI_FAILURE(Status))
    {
        ACPI_EXCEPTION((AE_INFO, Status, "While installing Notify handler"));
        /* Continue anyway, this is not critical */
    }

    Status = AcpiInstallNotifyHandler(ACPI_ROOT_OBJECT, ACPI_DEVICE_NOTIFY, NotifyHandler, NULL);
    if (ACPI_FAILURE(Status))
    {
        ACPI_EXCEPTION((AE_INFO, Status, "While installing Notify handler"));
        /* Continue anyway, this is not critical */
    }

    /* Note: Default address space handlers are installed by AcpiLoadTables() */
    /* We don't need to install custom handlers for basic table parsing */

    return (AE_OK);
}

static ACPI_STATUS acpi_enable_pic_mode(void)
{
    ACPI_STATUS status;
    ACPI_HANDLE handle = NULL;
    ACPI_OBJECT_LIST arg_list;
    ACPI_OBJECT arg[1];

    /* 1) 先确认 \_PIC 是否存在 */
    status = AcpiGetHandle(NULL, "\\_PIC", &handle);
    if (ACPI_FAILURE(status))
    {
        if (status == AE_NOT_FOUND)
        {
            KLOG_W("ACPI \\_PIC not found — firmware does not provide _PIC. Will use fallback.");
            /* 回退：屏蔽/初始化传统 PIC（如果适用） */

            /* 屏蔽主/从 PIC：写 0xFF 到 IMR (端口 0x21, 0xA1) */
            outb(0xff, 0x21);
            outb(0xff, 0xa1);
            KLOG_W("Fallback: masked legacy PICs (IMR=0xFF).");
            return AE_OK; /* 认为不是致命错误 */
        }
        else
        {
            KLOG_W("AcpiGetHandle(\"\\\\_PIC\") failed: 0x%x", (unsigned)status);
            return status;
        }
    }

    /* 2) 准备参数并调用 \_PIC(1) */
    arg_list.Count = 1;
    arg_list.Pointer = arg;

    arg[0].Type = ACPI_TYPE_INTEGER;
    arg[0].Integer.Value = 1; /* 1 = enable, 0 = disable (视固件实现) */

    status = AcpiEvaluateObject(handle, NULL, &arg_list, NULL);
    if (ACPI_FAILURE(status))
    {
        KLOG_W("error while executing \\_PIC: 0x%x\n", (unsigned)status);
        /* 这里也可以选择回退屏蔽 PIC，或返回错误由上层处理 */

        outb(0xff, 0x21);
        outb(0xff, 0xa1);
        KLOG_W("Fallback after \\_PIC failure: masked legacy PICs (IMR=0xFF).");
        /* 如果你希望把错误视为非致命，可以返回 AE_OK */
        return AE_OK;
    }
    else
    {
        KLOG_D("\\_PIC executed successfully (requested enable).");
    }

    return status;
}

/******************************************************************************
 *
 * FUNCTION:    acpica_init
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status (0 on success, negative on error)
 *
 * DESCRIPTION: Initialize ACPICA and dump ACPI table information
 *
 *****************************************************************************/

int acpica_init(void)
{
    ACPI_STATUS Status;
#ifdef ACPICA_DUMP_INFO
    ACPI_TABLE_HEADER *Table;
#endif

    Status = InitializeFullAcpica();

    if (ACPI_FAILURE(Status))
    {
        KLOG_E("Failed to initialize ACPICA: 0x%x", Status);
        return (ACPI_FAILURE(Status)) ? -1 : 0;
    }

#ifdef ACPICA_DUMP_INFO
    ListAllTables();

    /* Dump specific tables */
    KLOG_I("Step 8: Dumping specific ACPI tables");

    Status = AcpiGetTable(ACPI_SIG_RSDT, 1, &Table);
    if (ACPI_SUCCESS(Status))
    {
        KLOG_I("[RSDT] Root System Description Table");
        DumpTableInfo(Table);
    }
    else
    {
        KLOG_W("RSDT table not found: 0x%x", Status);
    }

    Status = AcpiGetTable(ACPI_SIG_XSDT, 1, &Table);
    if (ACPI_SUCCESS(Status))
    {
        KLOG_I("[XSDT] Extended System Description Table");
        DumpTableInfo(Table);
    }
    else
    {
        KLOG_W("XSDT table not found: 0x%x", Status);
    }

    Status = AcpiGetTable(ACPI_SIG_FADT, 1, &Table);
    if (ACPI_SUCCESS(Status))
    {
        DumpFadtTable((ACPI_TABLE_FADT *)Table);
    }
    else
    {
        KLOG_W("FADT table not found: 0x%x", Status);
    }

    Status = AcpiGetTable(ACPI_SIG_MADT, 1, &Table);
    if (ACPI_SUCCESS(Status))
    {
        DumpApicTable((ACPI_TABLE_MADT *)Table);
    }
    else
    {
        KLOG_W("MADT table not found: 0x%x", Status);
    }

    Status = AcpiGetTable(ACPI_SIG_MCFG, 1, &Table);
    if (ACPI_SUCCESS(Status))
    {
        DumpMcfgTable((ACPI_TABLE_MCFG *)Table);
    }
    else
    {
        KLOG_W("MCFG table not found: 0x%x", Status);
    }

    Status = AcpiGetTable(ACPI_SIG_SRAT, 1, &Table);
    if (ACPI_SUCCESS(Status))
    {
        DumpSratTable(Table);
    }
    else
    {
        KLOG_W("SRAT table not found: 0x%x", Status);
    }

    Status = AcpiGetTable(ACPI_SIG_HPET, 1, &Table);
    if (ACPI_SUCCESS(Status))
    {
        DumpHpetTable(Table);
    }
    else
    {
        KLOG_W("HPET table not found: 0x%x", Status);
    }

    Status = AcpiGetTable(ACPI_SIG_WAET, 1, &Table);
    if (ACPI_SUCCESS(Status))
    {
        KLOG_I("[WAET] Windows ACPI Emulated Devices Table");
        DumpTableInfo(Table);
    }
    else
    {
        KLOG_W("WAET table not found: 0x%x", Status);
    }

    Status = AcpiGetTable(ACPI_SIG_WSMT, 1, &Table);
    if (ACPI_SUCCESS(Status))
    {
        KLOG_I("[WSMT] Windows SMM Security Mitigations Table");
        DumpTableInfo(Table);
    }
    else
    {
        KLOG_W("WSMT table not found: 0x%x", Status);
    }
#endif

    Status = acpi_enable_pic_mode();
    if (ACPI_FAILURE(Status))
    {
        KLOG_E("Failed to Enable PIC mode: 0x%x", Status);
        return (ACPI_FAILURE(Status)) ? -1 : 0;
    }

    /* Enumerate DSDT devices */
    EnumerateDevices();
    return 0;
}
