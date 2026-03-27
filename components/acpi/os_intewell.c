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
#include <aclocal.h>
#include <actables.h>
#include <commonUtils.h>
#include <io.h>
#include <kmalloc.h>
#include <spinlock.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time/ktime.h>
#include <ttos.h>
#include <ttosMM.h>
#include <ttos_pic.h>
#include <uaccess.h>
#include <time/ktimer.h>
#include <sleep.h>
#if CONFIG_MULTIBOOT2
#include <multiboot.h>
#endif
#ifdef __x86_64__
#include <asm/io.h>
#endif

#ifdef CONFIG_SLUB
#include <slub.h>
#endif

#define KLOG_TAG "ACPI"
#include <klog.h>

/* Global variables */
static BOOLEAN AcpiOsInitialized = FALSE;

/* Search a block of memory for the RSDP signature. */
static uint8_t *scan_mem_for_rsdp(uint8_t *start, uint32_t length)
{
    ACPI_STATUS Status;
    ACPI_TABLE_RSDP *rsdp;
    uint8_t *address, *end;

    end = start + length;

    /* Search from given start address for the requested length */
    for (address = start; address < end; address += ACPI_RSDP_SCAN_STEP)
    {
        /*
         * Both RSDP signature and checksum must be correct.
         * Note: Sometimes there exists more than one RSDP in memory;
         * the valid RSDP has a valid checksum, all others have an
         * invalid checksum.
         */
        rsdp = (ACPI_TABLE_RSDP *)address;

        Status = AcpiTbValidateRsdp(rsdp);

        if (ACPI_FAILURE(Status))
            continue;

        /* Signature and checksum valid, we have found a real RSDP */
        return address;
    }
    return NULL;
}

/* Search RSDP address in EBDA. */
static phys_addr_t bios_get_rsdp_addr(void)
{
    unsigned long address;
    uint8_t *rsdp;
#if CONFIG_MULTIBOOT2
    /* Get the location of the Extended BIOS Data Area (EBDA) */
    address = multboot_get_acpi_table();
#else
    address = *(uint16_t *)ACPI_EBDA_PTR_LOCATION;
    address <<= 4;
#endif
    /*
     * Search EBDA paragraphs (EBDA is required to be a minimum of
     * 1K length)
     */
    if (address != 0)
    {
        rsdp = scan_mem_for_rsdp((uint8_t *)address, ACPI_EBDA_WINDOW_SIZE);
        if (rsdp)
            return (phys_addr_t)(unsigned long)rsdp;
    }

    /* Search upper memory: 16-byte boundaries in E0000h-FFFFFh */
    rsdp = scan_mem_for_rsdp((uint8_t *)ACPI_HI_RSDP_WINDOW_BASE, ACPI_HI_RSDP_WINDOW_SIZE);
    if (rsdp)
        return (phys_addr_t)(unsigned long)rsdp;

    return 0;
}

/******************************************************************************
 *
 * FUNCTION:    AcpiOsGetRootPointer
 *
 * PARAMETERS:  None
 *
 * RETURN:      RSDP physical address
 *
 * DESCRIPTION: Gets the ACPI root pointer (RSDP)
 *
 *****************************************************************************/

ACPI_PHYSICAL_ADDRESS
AcpiOsGetRootPointer(void)
{
    return (ACPI_PHYSICAL_ADDRESS)bios_get_rsdp_addr();
}

/******************************************************************************
 *
 * FUNCTION:    AcpiOsInitialize
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Initialize the OS-specific portions of ACPICA
 *
 *****************************************************************************/

ACPI_STATUS ACPI_INIT_FUNCTION AcpiOsInitialize(void)
{
    AcpiOsInitialized = TRUE;
    return (AE_OK);
}

/******************************************************************************
 *
 * FUNCTION:    AcpiOsTerminate
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Shutdown the OS-specific portions of ACPICA
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsTerminate(void)
{
    AcpiOsInitialized = FALSE;
    return (AE_OK);
}

/******************************************************************************
 *
 * FUNCTION:    AcpiOsAllocate
 *
 * PARAMETERS:  Size                - Amount to allocate
 *
 * RETURN:      Pointer to the new allocation
 *
 * DESCRIPTION: Allocate memory
 *
 *****************************************************************************/

void *AcpiOsAllocate(ACPI_SIZE Size)
{
    return (kmalloc(Size, GFP_KERNEL));
}

/******************************************************************************
 *
 * FUNCTION:    AcpiOsFree
 *
 * PARAMETERS:  Memory              - Pointer to memory to free
 *
 * RETURN:      None
 *
 * DESCRIPTION: Free memory
 *
 *****************************************************************************/

void AcpiOsFree(void *Memory)
{
    kfree(Memory);
}

/******************************************************************************
 *
 * FUNCTION:    AcpiOsGetThreadId
 *
 * PARAMETERS:  None
 *
 * RETURN:      Thread ID
 *
 * DESCRIPTION: Get current thread ID
 *
 *****************************************************************************/

ACPI_THREAD_ID
AcpiOsGetThreadId(void)
{
    return (ACPI_THREAD_ID)(unsigned long)ttosGetCurrentCpuRunningTask();
}

/******************************************************************************
 *
 * FUNCTION:    AcpiOsPrintf
 *
 * PARAMETERS:  Fmt, ...            - Standard printf format
 *
 * RETURN:      None
 *
 * DESCRIPTION: Formatted output
 *
 *****************************************************************************/

void ACPI_INTERNAL_VAR_XFACE AcpiOsPrintf(const char *Fmt, ...)
{
    va_list args;

    va_start(args, Fmt);
    AcpiOsVprintf(Fmt, args);
    va_end(args);
}

/******************************************************************************
 *
 * FUNCTION:    AcpiOsVprintf
 *
 * PARAMETERS:  Fmt                 - Standard printf format
 *              Args                - Argument list
 *
 * RETURN:      None
 *
 * DESCRIPTION: Formatted output with argument list
 *
 *****************************************************************************/

void AcpiOsVprintf(const char *Fmt, va_list Args)
{
#ifdef CONFIG_ACPI_DEBUG
    vprintk(Fmt, Args);
#endif
}

#ifndef ACPI_USE_NATIVE_MEMORY_MAPPING
/******************************************************************************
 *
 * FUNCTION:    AcpiOsMapMemory
 *
 * PARAMETERS:  Where               - Physical address of memory to be mapped
 *              Length              - How much memory to map
 *
 * RETURN:      Pointer to mapped memory. Null on error.
 *
 * DESCRIPTION: Map physical memory into caller's address space
 *
 *****************************************************************************/

void *AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS Where, ACPI_SIZE Length)
{
    ACPI_PHYSICAL_ADDRESS Offset;
    Offset = Where % PAGE_SIZE;
    void *vaddr = (void *)ioremap(Where - Offset, Length + Offset);
    if (!vaddr)
    {
        KLOG_E("AcpiOsMapMemory: ioremap failed");
        return NULL;
    }
    return (vaddr + Offset);
}

/******************************************************************************
 *
 * FUNCTION:    AcpiOsUnmapMemory
 *
 * PARAMETERS:  LogicalAddress      - Virtual address
 *              Size                - Size of mapped memory
 *
 * RETURN:      None
 *
 * DESCRIPTION: Unmap physical memory
 *
 *****************************************************************************/

void AcpiOsUnmapMemory(void *LogicalAddress, ACPI_SIZE Size)
{
    virt_addr_t Offset;
    Offset = (virt_addr_t)LogicalAddress % PAGE_SIZE;

    struct mm_region region;
    mm_region_init(&region, 0, 0, Size + Offset);
    region.virtual_address = (virt_addr_t)LogicalAddress - Offset;
    mm_region_modify(get_kernel_mm(), &region, true);
}
#endif

/******************************************************************************
 *
 * FUNCTION:    AcpiOsGetPhysicalAddress
 *
 * PARAMETERS:  LogicalAddress      - Virtual address
 *              PhysicalAddress     - Physical address
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Get physical address from virtual address
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsGetPhysicalAddress(void *LogicalAddress, ACPI_PHYSICAL_ADDRESS *PhysicalAddress)
{
    *PhysicalAddress = mm_kernel_v2p((virt_addr_t)LogicalAddress);
    return (AE_OK);
}

/******************************************************************************
 *
 * FUNCTION:    AcpiOsGetTimer
 *
 * PARAMETERS:  None
 *
 * RETURN:      Current time in 100-nanosecond units
 *
 * DESCRIPTION: Get system timer value
 *
 *****************************************************************************/

UINT64
AcpiOsGetTimer(void)
{
    struct timespec64 ts;
    kernel_clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_nsec + ts.tv_sec * NSEC_PER_SEC) / 10;
}

/******************************************************************************
 *
 * FUNCTION:    AcpiOsStall
 *
 * PARAMETERS:  Microseconds        - How long to stall
 *
 * RETURN:      None
 *
 * DESCRIPTION: Sleep for specified microseconds
 *
 *****************************************************************************/

void AcpiOsStall(UINT32 Microseconds)
{
    usleep(Microseconds * USEC_PER_MSEC);
}

/******************************************************************************
 *
 * FUNCTION:    AcpiOsSleep
 *
 * PARAMETERS:  Milliseconds        - How long to sleep
 *
 * RETURN:      None
 *
 * DESCRIPTION: Sleep for specified milliseconds
 *
 *****************************************************************************/

void AcpiOsSleep(UINT64 Milliseconds)
{
    msleep(Milliseconds);
}

/******************************************************************************
 *
 * FUNCTION:    AcpiOsReadPort
 *
 * PARAMETERS:  Address             - I/O port address
 *              Value               - Where to return the value
 *              Width               - Number of bits to read
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Read from an I/O port
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsReadPort(ACPI_IO_ADDRESS Address, UINT32 *Value, UINT32 Width)
{
    if (!Value)
    {
        return AE_BAD_PARAMETER;
    }
    switch (Width)
    {
    case 8:
#ifdef __x86_64__
        inb(Address, *Value);
#else
        *Value = readb(Address);
#endif
        break;
    case 16:
#ifdef __x86_64__
        inw(Address, *Value);
#else
        *Value = readw(Address);
#endif
        break;
    case 32:
#ifdef __x86_64__
        inl(Address, *Value);
#else
        *Value = readl(Address);
#endif
        break;
    default:
        return AE_BAD_PARAMETER;
    }
    return (AE_OK);
}

/******************************************************************************
 *
 * FUNCTION:    AcpiOsWritePort
 *
 * PARAMETERS:  Address             - I/O port address
 *              Value               - Value to write
 *              Width               - Number of bits to write
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Write to an I/O port
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsWritePort(ACPI_IO_ADDRESS Address, UINT32 Value, UINT32 Width)
{
    switch (Width)
    {
    case 8:
#ifdef __x86_64__
        outb(Address, Value);
#else
        writeb(Value, Address);
#endif
        break;
    case 16:
#ifdef __x86_64__
        outw(Address, Value);
#else
        writew(Value, Address);
#endif
        break;
    case 32:
#ifdef __x86_64__
        outl(Address, Value);
#else
        writel(Value, Address);
#endif
        break;
    default:
        return AE_BAD_PARAMETER;
    }
    return (AE_OK);
}

/******************************************************************************
 *
 * FUNCTION:    AcpiOsReadMemory
 *
 * PARAMETERS:  Address             - Physical memory address
 *              Value               - Where to return the value
 *              Width               - Number of bits to read
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Read from physical memory
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsReadMemory(ACPI_PHYSICAL_ADDRESS Address, UINT64 *Value, UINT32 Width)
{
    void *LogicalAddress = AcpiOsMapMemory(Address, Width / 8);
    if (!LogicalAddress)
    {
        return (AE_BAD_ADDRESS);
    }

    switch (Width)
    {
    case 8:
        *Value = *(UINT8 volatile *)LogicalAddress;
        break;
    case 16:
        *Value = *(UINT16 volatile *)LogicalAddress;
        break;
    case 32:
        *Value = *(UINT32 volatile *)LogicalAddress;
        break;
    case 64:
        *Value = *(UINT64 volatile *)LogicalAddress;
        break;
    default:
        return (AE_BAD_PARAMETER);
    }

    AcpiOsUnmapMemory(LogicalAddress, Width / 8);
    return (AE_OK);
}

/******************************************************************************
 *
 * FUNCTION:    AcpiOsWriteMemory
 *
 * PARAMETERS:  Address             - Physical memory address
 *              Value               - Value to write
 *              Width               - Number of bits to write
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Write to physical memory
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsWriteMemory(ACPI_PHYSICAL_ADDRESS Address, UINT64 Value, UINT32 Width)
{
    void *LogicalAddress = AcpiOsMapMemory(Address, Width / 8);
    if (!LogicalAddress)
    {
        return (AE_BAD_ADDRESS);
    }

    switch (Width)
    {
    case 8:
        *(UINT8 volatile *)LogicalAddress = (UINT8)Value;
        break;
    case 16:
        *(UINT16 volatile *)LogicalAddress = (UINT16)Value;
        break;
    case 32:
        *(UINT32 volatile *)LogicalAddress = (UINT32)Value;
        break;
    case 64:
        *(UINT64 volatile *)LogicalAddress = (UINT64)Value;
        break;
    default:
        return (AE_BAD_PARAMETER);
    }

    AcpiOsUnmapMemory(LogicalAddress, Width / 8);
    return (AE_OK);
}

BOOLEAN AcpiOsReadable(void *Pointer, ACPI_SIZE Length)
{
    return kernel_access_check(Pointer, Length, UACCESS_R) ? TRUE : FALSE;
}

#ifdef USE_NATIVE_ALLOCATE_ZEROED
/******************************************************************************
 *
 * FUNCTION:    AcpiOsAllocateZeroed
 *
 * PARAMETERS:  Size                - Amount to allocate, in bytes
 *
 * RETURN:      Pointer to the new allocation. Null on error.
 *
 * DESCRIPTION: Allocate and zero memory. Algorithm is dependent on the OS.
 *
 *****************************************************************************/

void *AcpiOsAllocateZeroed(ACPI_SIZE Size)
{
    return kzalloc(Size, GFP_KERNEL);
}
#endif

/******************************************************************************
 *
 * FUNCTION:    AcpiOsCreateLock
 *
 * PARAMETERS:  OutHandle           - Pointer to handle
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create a spinlock
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsCreateLock(ACPI_SPINLOCK *OutHandle)
{
    ttos_spinlock_t *Lock;

    Lock = AcpiOsAllocate(sizeof(ttos_spinlock_t));
    if (!Lock)
    {
        return (AE_NO_MEMORY);
    }

    spin_lock_init(Lock);
    *OutHandle = (ACPI_SPINLOCK)Lock;
    return (AE_OK);
}

/******************************************************************************
 *
 * FUNCTION:    AcpiOsDeleteLock
 *
 * PARAMETERS:  Handle              - Spinlock handle
 *
 * RETURN:      None
 *
 * DESCRIPTION: Delete a spinlock
 *
 *****************************************************************************/

void AcpiOsDeleteLock(ACPI_SPINLOCK Handle)
{
    if (Handle)
    {
        AcpiOsFree(Handle);
    }
}

/******************************************************************************
 *
 * FUNCTION:    AcpiOsAcquireLock
 *
 * PARAMETERS:  Handle              - Spinlock handle
 *
 * RETURN:      CPU flags
 *
 * DESCRIPTION: Acquire a spinlock
 *
 *****************************************************************************/

ACPI_CPU_FLAGS
AcpiOsAcquireLock(ACPI_SPINLOCK Handle)
{
    irq_flags_t irq_flag;
    spin_lock_irqsave((ttos_spinlock_t *)Handle, irq_flag);
    return ((ACPI_CPU_FLAGS)irq_flag);
}

/******************************************************************************
 *
 * FUNCTION:    AcpiOsReleaseLock
 *
 * PARAMETERS:  Handle              - Spinlock handle
 *              Flags               - CPU flags
 *
 * RETURN:      None
 *
 * DESCRIPTION: Release a spinlock
 *
 *****************************************************************************/

void AcpiOsReleaseLock(ACPI_SPINLOCK Handle, ACPI_CPU_FLAGS Flags)
{
    irq_flags_t irq_flag = (irq_flags_t)Flags;
    spin_unlock_irqrestore((ttos_spinlock_t *)Handle, irq_flag);
}

/******************************************************************************
 *
 * FUNCTION:    AcpiOsCreateSemaphore
 *
 * PARAMETERS:  MaxUnits            - Maximum units
 *              InitialUnits        - Initial units
 *              OutHandle           - Pointer to handle
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create a semaphore (stub implementation)
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsCreateSemaphore(UINT32 MaxUnits, UINT32 InitialUnits, ACPI_SEMAPHORE *OutHandle)
{
    SEMA_ID sema_id;
    T_TTOS_ReturnCode ret;

    ret = TTOS_CreateSemaEx(InitialUnits, &sema_id);
    if (TTOS_OK != ret)
    {
        KLOG_E("AcpiOsCreateSemaphore: CreateSemaEx failed, ret=%d", ret);
        return (AE_ERROR);
    }
    *OutHandle = (ACPI_SEMAPHORE)sema_id;
    return (AE_OK);
}

/******************************************************************************
 *
 * FUNCTION:    AcpiOsDeleteSemaphore
 *
 * PARAMETERS:  Handle              - Semaphore handle
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Delete a semaphore
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsDeleteSemaphore(ACPI_SEMAPHORE Handle)
{
    SEMA_ID sema_id = (SEMA_ID)Handle;
    TTOS_DeleteSema(sema_id);
    return (AE_OK);
}

/******************************************************************************
 *
 * FUNCTION:    AcpiOsWaitSemaphore
 *
 * PARAMETERS:  Handle              - Semaphore handle
 *              Units               - Units to wait
 *              Timeout             - Timeout in milliseconds
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Wait for a semaphore
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsWaitSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units, UINT16 Timeout)
{
    if (!Handle)
    {
        return (AE_BAD_PARAMETER);
    }
    T_TTOS_ReturnCode ret = TTOS_ObtainSemaUninterruptable(
        (SEMA_ID)Handle, Timeout == ACPI_WAIT_FOREVER ? TTOS_WAIT_FOREVER : Timeout);

    if (TTOS_OK != ret)
    {
        KLOG_E("AcpiOsWaitSemaphore: ObtainSemaUninterruptable failed, ret=%d", ret);
        return (AE_ERROR);
    }
    return (AE_OK);
}

/******************************************************************************
 *
 * FUNCTION:    AcpiOsSignalSemaphore
 *
 * PARAMETERS:  Handle              - Semaphore handle
 *              Units               - Units to signal
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Signal a semaphore
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsSignalSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units)
{
    if (!Handle)
    {
        return (AE_BAD_PARAMETER);
    }
    TTOS_ReleaseSema((SEMA_ID)Handle);
    return (AE_OK);
}

/******************************************************************************
 *
 * FUNCTION:    AcpiOsSignal
 *
 * PARAMETERS:  Function            - Signal function
 *              Info                - Signal info
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Signal handler (stub implementation)
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsSignal(UINT32 Function, void *Info)
{
    switch (Function)
    {
    case ACPI_SIGNAL_FATAL:
        KLOG_I("ACPI_SIGNAL_FATAL error");
        break;

    case ACPI_SIGNAL_BREAKPOINT:
        KLOG_I("ACPI_SIGNAL_BREAKPOINT");
        break;

    default:
        break;
    }
    return (AE_OK);
}

/******************************************************************************
 *
 * FUNCTION:    AcpiOsExecute
 *
 * PARAMETERS:  Type                - Execution type
 *              Function            - Function to execute
 *              Context             - Context for function
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute function asynchronously (stub implementation)
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsExecute(ACPI_EXECUTE_TYPE Type, ACPI_OSD_EXEC_CALLBACK Function, void *Context)
{
    /* Stub implementation - just call the function directly */
    Function(Context);
    KLOG_I("AcpiOsExecute: Type=%d, Function=%p, Context=%p", Type, Function, Context);
    return (AE_OK);
}

/******************************************************************************
 *
 * FUNCTION:    AcpiOsReadPciConfiguration
 *
 * PARAMETERS:  PciId               - PCI device ID
 *              Register            - Register offset
 *              Value               - Where to return the value
 *              Width               - Number of bits to read
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Read from PCI configuration space (stub implementation)
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsReadPciConfiguration(ACPI_PCI_ID *PciId, UINT32 Register, UINT64 *Value, UINT32 Width)
{
    *Value = 0;
    // todo
    //  KLOG_E("AcpiOsReadPciConfiguration: PciId=%p, Register=%d, Value=%p, Width=%d", PciId,
    //  Register,
    //         Value, Width);
    return (AE_OK);
}

/******************************************************************************
 *
 * FUNCTION:    AcpiOsWritePciConfiguration
 *
 * PARAMETERS:  PciId               - PCI device ID
 *              Register            - Register offset
 *              Value               - Value to write
 *              Width               - Number of bits to write
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Write to PCI configuration space (stub implementation)
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsWritePciConfiguration(ACPI_PCI_ID *PciId, UINT32 Register, UINT64 Value, UINT32 Width)
{
    // todo
    //  KLOG_E("AcpiOsWritePciConfiguration: PciId=%p, Register=%d, Value=%d, Width=%d", PciId,
    //         Register, Value, Width);
    return (AE_OK);
}

/******************************************************************************
 *
 * FUNCTION:    AcpiOsInstallInterruptHandler
 *
 * PARAMETERS:  InterruptNumber     - Interrupt number
 *              ServiceRoutine      - Service routine
 *              Context             - Context for routine
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Install interrupt handler (stub implementation)
 *
 *****************************************************************************/
#define MAX_IRQ 256

struct irq_info
{
    UINT32 virq;
    ACPI_OSD_HANDLER handle;
    void *Context;
} irq_table[MAX_IRQ];

static void irq_handle(uint32_t irq, void *arg)
{
    struct irq_info *info = (struct irq_info *)arg;
    if (info->handle)
    {
        KLOG_I("ACPI irq:%d", irq);
        info->handle(info->Context);
    }
}

ACPI_STATUS
AcpiOsInstallInterruptHandler(UINT32 InterruptNumber, ACPI_OSD_HANDLER ServiceRoutine,
                              void *Context)
{
    if (InterruptNumber > MAX_IRQ)
    {
        return (AE_BAD_PARAMETER);
    }
    irq_table[InterruptNumber].virq = ttos_pic_irq_alloc(NULL, InterruptNumber);
    irq_table[InterruptNumber].Context = Context;
    irq_table[InterruptNumber].handle = ServiceRoutine;

    ttos_pic_irq_install(irq_table[InterruptNumber].virq, irq_handle, &irq_table[InterruptNumber],
                         0, "ACPI");
    if (9 != irq_table[InterruptNumber].virq)
        ttos_pic_irq_unmask(irq_table[InterruptNumber].virq);

    return (AE_OK);
}

/******************************************************************************
 *
 * FUNCTION:    AcpiOsRemoveInterruptHandler
 *
 * PARAMETERS:  InterruptNumber     - Interrupt number
 *              ServiceRoutine      - Service routine
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Remove interrupt handler (stub implementation)
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsRemoveInterruptHandler(UINT32 InterruptNumber, ACPI_OSD_HANDLER ServiceRoutine)
{
    if (InterruptNumber > MAX_IRQ)
    {
        return (AE_BAD_PARAMETER);
    }
    ttos_pic_irq_mask(irq_table[InterruptNumber].virq);
    ttos_pic_irq_uninstall(irq_table[InterruptNumber].virq, "ACPI");
    irq_table[InterruptNumber].virq = 0;
    irq_table[InterruptNumber].Context = NULL;
    irq_table[InterruptNumber].handle = NULL;
    return (AE_OK);
}

/******************************************************************************
 *
 * FUNCTION:    AcpiOsCreateCache
 *
 * PARAMETERS:  CacheName           - Cache name
 *              ObjectSize          - Object size
 *              MaxDepth            - Maximum depth
 *              ReturnCache         - Where to return cache handle
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create a memory cache (stub implementation)
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsCreateCache(char *CacheName, UINT16 ObjectSize, UINT16 MaxDepth, ACPI_CACHE_T **ReturnCache)
{
    ACPI_CACHE_T *Cache;
#ifdef CONFIG_SLUB
    Cache = (ACPI_CACHE_T *)kmem_cache_create(CacheName, ObjectSize, 0, 0, NULL);
    if (!Cache)
    {
        return (AE_NO_MEMORY);
    }
    *ReturnCache = Cache;
#else
    *ReturnCache = (ACPI_CACHE_T *)(uintptr_t)ObjectSize;
#endif
    return (AE_OK);
}

/******************************************************************************
 *
 * FUNCTION:    AcpiOsDeleteCache
 *
 * PARAMETERS:  Cache               - Cache handle
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Delete a memory cache
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsDeleteCache(ACPI_CACHE_T *Cache)
{
#ifdef CONFIG_SLUB
    if (Cache)
    {
        kmem_cache_destroy((struct kmem_cache *)Cache);
    }
#endif
    return (AE_OK);
}

/******************************************************************************
 *
 * FUNCTION:    AcpiOsAcquireObject
 *
 * PARAMETERS:  Cache               - Cache handle
 *
 * RETURN:      Pointer to object
 *
 * DESCRIPTION: Acquire object from cache
 *
 *****************************************************************************/

void *AcpiOsAcquireObject(ACPI_CACHE_T *Cache)
{
    void *object;
    if (!Cache)
    {
        return (NULL);
    }
#ifdef CONFIG_SLUB
    struct kmem_cache *slub_cache = (struct kmem_cache *)Cache;
    object = kmem_cache_alloc(slub_cache, GFP_KERNEL);
    memset(object, 0, slub_cache->object_size);
#else
    UINT16 ObjectSize = (UINT16)(uintptr_t)Cache;
    object = AcpiOsAllocate(ObjectSize);
    memset(object, 0, ObjectSize);
#endif
    return object;
}

/******************************************************************************
 *
 * FUNCTION:    AcpiOsReleaseObject
 *
 * PARAMETERS:  Cache               - Cache handle
 *              Object              - Object to release
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Release object back to cache
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsReleaseObject(ACPI_CACHE_T *Cache, void *Object)
{
    if (!Cache)
    {
        return (AE_BAD_PARAMETER);
    }

    if (Object)
    {
#ifdef CONFIG_SLUB
        struct kmem_cache *slub_cache = (struct kmem_cache *)Cache;
        kmem_cache_free(slub_cache, Object);
#else
        AcpiOsFree(Object);
#endif
    }

    return (AE_OK);
}

/******************************************************************************
 *
 * FUNCTION:    AcpiOsTableOverride
 *
 * PARAMETERS:  ExistingTable       - Existing table
 *              NewTable            - New table
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Override ACPI table (stub implementation)
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsTableOverride(ACPI_TABLE_HEADER *ExistingTable, ACPI_TABLE_HEADER **NewTable)
{
    if (!ExistingTable || !NewTable)
    {
        return (AE_BAD_PARAMETER);
    }

    *NewTable = NULL;

    return (AE_NO_ACPI_TABLES);
}

/******************************************************************************
 *
 * FUNCTION:    AcpiOsPhysicalTableOverride
 *
 * PARAMETERS:  ExistingTable       - Existing table
 *              NewAddress          - New address
 *              NewTableLength      - New table length
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Override ACPI table with physical address (stub implementation)
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsPhysicalTableOverride(ACPI_TABLE_HEADER *ExistingTable, ACPI_PHYSICAL_ADDRESS *NewAddress,
                            UINT32 *NewTableLength)
{
    return (AE_SUPPORT);
}

/******************************************************************************
 *
 * FUNCTION:    AcpiOsPredefinedOverride
 *
 * PARAMETERS:  InitVal             - Initial value
 *              NewVal              - New value
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Override predefined object (stub implementation)
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsPredefinedOverride(const ACPI_PREDEFINED_NAMES *InitVal, ACPI_STRING *NewVal)
{
    if (!InitVal || !NewVal)
    {
        return (AE_BAD_PARAMETER);
    }

    *NewVal = NULL;
    return (AE_OK);
}

/******************************************************************************
 *
 * FUNCTION:    AcpiOsPurgeCache
 *
 * PARAMETERS:  Cache               - Cache handle
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Purge a memory cache (stub implementation)
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsPurgeCache(ACPI_CACHE_T *Cache)
{
    return (AE_OK);
}

/******************************************************************************
 *
 * FUNCTION:    AcpiOsWaitEventsComplete
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Wait for all asynchronous events to complete. This
 *              implementation does nothing.
 *
 *****************************************************************************/

void AcpiOsWaitEventsComplete(void)
{
    return;
}

/******************************************************************************
 *
 * FUNCTION:    AcpiOsEnterSleep
 *
 * PARAMETERS:  SleepState          - Which sleep state to enter
 *              RegaValue           - Register A value
 *              RegbValue           - Register B value
 *
 * RETURN:      Status
 *
 * DESCRIPTION: A hook before writing sleep registers to enter the sleep
 *              state. Return AE_CTRL_SKIP to skip further sleep register
 *              writes.
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsEnterSleep(UINT8 SleepState, UINT32 RegaValue, UINT32 RegbValue)
{
    return (AE_OK);
}
