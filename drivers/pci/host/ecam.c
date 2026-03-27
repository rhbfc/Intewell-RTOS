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
#include <io.h>
#include <ttos_init.h>
#include <ttos_pic.h>

#if defined(__x86_64__)
#include <x86_pci_io.h>
#endif

#define KLOG_TAG "ECAM"
#include <klog.h>

#define IS_ALIGNED(x, a) (((x) & ((a)-1)) == 0)

static inline uint64_t ecam_cfg_address_get(struct pci_host *host, uint32_t bdf, uint32_t offset)
{
    return host->cfg.vaddr + (bdf << 12) + (offset & 0xFFF);
}

static uint32_t ecam_generic_rw(struct pci_host *host, uint32_t bdf, uint32_t offset,
                                uint32_t value, uint32_t bytes, bool is_write)
{
    intptr_t address = (intptr_t)ecam_cfg_address_get(host, bdf, offset);

    bytes |= is_write ? 0x80 : 0;

    switch (bytes)
    {
    case 0x01:
        value = readb(address);
        break;
    case 0x02:
        value = readw(address);
        break;
    case 0x04:
        value = readl(address);
        break;

    case 0x81:
        writeb(value, address);
        break;
    case 0x82:
        writew(value, address);
        break;
    case 0x84:
        writel(value, address);
        break;
    default:
        value = 0;
        break;
    }

    // printk("%s addr: %p  val: 0x%X\n", is_write ? "write" : "read ",
    //         address, value);

    return value;
}

static int ecam_read_io(struct pci_host *bus, uintptr_t addr, int size, uint32_t *val)
{
    if (!IS_ALIGNED(addr, size))
    {
        return -EINVAL;
    }

    switch (size)
    {
    case 0x01:
        *val = readb(addr);
        break;
    case 0x02:
        *val = readw(addr);
        break;
    case 0x04:
        *val = readl(addr);
        break;
    default:
        *val = 0;
        break;
    }

    return 0;
}

static int ecam_write_io(struct pci_host *bus, uintptr_t addr, int size, uint32_t val)
{
    if (!IS_ALIGNED(addr, size))
    {
        return -EINVAL;
    }

    switch (size)
    {
    case 0x1:
        writeb(val, addr);
        break;
    case 0x2:
        writew(val, addr);
        break;
    case 0x4:
        writel(val, addr);
        break;
    default:
        val = 0;
        break;
    }

    return 0;
}

static struct pci_ops ecam_generic_ops = {
    .cfg_rw = ecam_generic_rw,
#ifdef __x86_64__
    .read_io = x86_64_pci_read_io,
    .write_io = x86_64_pci_write_io,
#else
    .read_io = ecam_read_io,
    .write_io = ecam_write_io,
#endif
};

static int ecam_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
#ifdef CONFIG_ACPI
    bool pnp_pci_vaild(struct platform_device * pdev);
    if (!pnp_pci_vaild(pdev))
    {
        return -ENODEV;
    }
#endif
    struct pci_host *host = devm_kzalloc(dev, sizeof(struct pci_host), GFP_KERNEL);

    host->dev.parent = dev;
    host->ops = &ecam_generic_ops;

    return pci_host_register(host);
}

static struct of_device_id pci_of_ecam_table[] = {
    {.compatible = "pci-host-ecam-generic"},
    {/* end of list */},
};

static const struct acpi_device_id pci_acpi_ecam_table[] = {
    {"PNP0A08", 0},
    {/* end of list */},
};

static struct platform_driver ecam_generic_driver = {
    .probe = ecam_probe,
    .driver =
        {
            .name = "ecam-generic",
            .of_match_table = pci_of_ecam_table,
            .acpi_match_table = pci_acpi_ecam_table,
        },
};

PCI_HOST_DRIVER_EXPORT(ecam_generic_driver);
