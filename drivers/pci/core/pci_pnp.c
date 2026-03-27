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

#include <driver/pci/pci_host.h>
#include <driver/platform.h>
#include <stdlib.h>
#include <ttosMM.h>
#include <ttos_pic.h>

#ifdef CONFIG_ACPI

#include <acpi.h>
#include <driver/acpi.h>

void *msi_pic_get(void);

static ACPI_MCFG_ALLOCATION *pnp_pci_get_mcfg(struct platform_device *pdev)
{
    UINT16 uid;
    ACPI_STATUS Status;
    ACPI_TABLE_MCFG *Mcfg;
    ACPI_MCFG_ALLOCATION *Allocation;
    if (pdev->adev.dev_info->Valid & ACPI_VALID_UID)
    {
        uid = atoi(pdev->adev.dev_info->UniqueId.String);

        Status = AcpiGetTable(ACPI_SIG_MCFG, 1, (ACPI_TABLE_HEADER **)&Mcfg);
        if (ACPI_FAILURE(Status))
        {
            return NULL;
        }
        Allocation = (ACPI_MCFG_ALLOCATION *)((UINT8 *)Mcfg + sizeof(ACPI_TABLE_MCFG));
        while (((UINT8 *)Allocation) < ((UINT8 *)Mcfg + Mcfg->Header.Length))
        {
            if (uid == Allocation->PciSegment)
            {
                return Allocation;
            }
            Allocation++;
        }
    }
    return NULL;
}

bool pnp_pci_vaild(struct platform_device *pdev)
{
    return !!pnp_pci_get_mcfg(pdev);
}

int pci_host_pnp_parmes(struct pci_host *host)
{
    struct pci_iomem mem32, mem64, pre_mem32, pre_mem64;
    struct device *dev = host->dev.parent;
    struct platform_device *pdev = to_platform_device(dev);

    ACPI_MCFG_ALLOCATION *Allocation;
    ACPI_RESOURCE *resource;
    ACPI_PCI_ROUTING_TABLE *Prt;

    Allocation = pnp_pci_get_mcfg(pdev);
    host->bus_ranges.start = Allocation->StartBusNumber;
    host->bus_ranges.end = Allocation->EndBusNumber;

    host->cfg.paddr = Allocation->Address;
    host->cfg.size = (Allocation->EndBusNumber - Allocation->StartBusNumber + 1) * 0x10000;
    host->flag = PCI_HOST_FLAG_PROBE_ONLY;
    host->pic = msi_pic_get();

    memset(&mem32, 0, sizeof(struct pci_iomem));
    memset(&mem64, 0, sizeof(struct pci_iomem));
    memset(&pre_mem32, 0, sizeof(struct pci_iomem));
    memset(&pre_mem64, 0, sizeof(struct pci_iomem));

    resource = acpi_device_get_resource(&pdev->adev, ACPI_RESOURCE_TYPE_IO);

    if (resource)
    {
        host->io.pci.addr = resource->Data.Io.Minimum;
        host->io.pci.size = resource->Data.Io.AddressLength;
        host->io.cpu.addr = resource->Data.Io.Minimum;
        host->io.cpu.size = resource->Data.Io.AddressLength;
    }

    resource = acpi_device_get_resource(&pdev->adev, ACPI_RESOURCE_TYPE_ADDRESS32);
    if (resource)
    {
        host->mem.pci.addr = resource->Data.Address32.Address.Minimum;
        host->mem.pci.size = resource->Data.Address32.Address.AddressLength;
        host->mem.cpu.addr = resource->Data.Address32.Address.Minimum;
        host->mem.cpu.size = resource->Data.Address32.Address.AddressLength;
    }

    resource = acpi_device_get_resource(&pdev->adev, ACPI_RESOURCE_TYPE_ADDRESS64);
    if (resource)
    {
        host->mem.pci.addr = resource->Data.Address64.Address.Minimum;
        host->mem.pci.size = resource->Data.Address64.Address.AddressLength;
        host->mem.cpu.addr = resource->Data.Address64.Address.Minimum;
        host->mem.cpu.size = resource->Data.Address64.Address.AddressLength;
    }

    if (mem32.pci.size)
    {
        host->mem = mem32;
    }
    else if (mem64.pci.size)
    {
        host->mem = mem64;
    }
    /* todo */
    if (pre_mem64.pci.size)
    {
        host->pre_mem = pre_mem64;
    }
    else if (pre_mem32.pci.size)
    {
        host->pre_mem = pre_mem32;
    }

    Prt = acpi_device_get_prt(&pdev->adev);

    if (Prt)
    {
        if (!Prt[0].Source[0])
        {
            for (int i = 0; i < 4; i++)
            {
                if (!Prt[i].SourceIndex)
                {
                    break;
                }
                host->legacy_irq[Prt[i].Pin] = ttos_pic_irq_alloc(NULL, Prt[i].SourceIndex);
#ifdef PCI_DEBUG
                printk("Prt->Address: 0x%" PRIx64 "\n", Prt[i].Address);
                printk("Prt->Pin: %d\n", Prt[i].Pin);
                printk("Prt->Source: %s\n", Prt[i].Source);
                printk("Prt->SourceIndex: %d\n", Prt[i].SourceIndex);
#endif
            }
        }
    }

#ifdef PCI_DEBUG
    printk("host->cfg.paddr          0x%" PRIx64 "\n", host->cfg.paddr);
    printk("host->cfg.size           0x%lX\n", host->cfg.size);
    printk("host->io.cpu.addr        0x%" PRIx64 "\n", host->io.cpu.addr);
    printk("host->io.pci.addr        0x%" PRIx64 "\n", host->io.pci.addr);
    printk("host->io.pci.size        0x%" PRIx64 "\n", host->io.pci.size);
    printk("host->mem.cpu.addr       0x%" PRIx64 "\n", host->mem.cpu.addr);
    printk("host->mem.pci.addr       0x%" PRIx64 "\n", host->mem.pci.addr);
    printk("host->mem.pci.size       0x%" PRIx64 "\n", host->mem.pci.size);
    printk("host->pre_mem.cpu.addr   0x%" PRIx64 "\n", host->pre_mem.cpu.addr);
    printk("host->pre_mem.pci.addr   0x%" PRIx64 "\n", host->pre_mem.pci.addr);
    printk("host->pre_mem.pci.size   0x%" PRIx64 "\n", host->pre_mem.pci.size);
#endif

    if (host->cfg.paddr && host->cfg.size)
    {
        host->cfg.vaddr = (virt_addr_t)ioremap(host->cfg.paddr, host->cfg.size);
    }

    return 0;
}
#else
int pci_host_pnp_parmes(struct pci_host *host)
{
    return 0;
}
#endif /* CONFIG_ACPI */
