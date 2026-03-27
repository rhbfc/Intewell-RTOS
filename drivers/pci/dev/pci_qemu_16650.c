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

#include <driver/pci/pci.h>
#include <ttos_pic.h>
#include <io.h>

static void *base_mmio;

static void serial_handler(uint32_t irq, void *param)
{
    printk("%s: %p %c\n", __func__, param, readb(param));
}

static int serial_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
    base_mmio = pci_bar_ioremap(dev, 0);
    pci_dev_print(dev, " vendor: %X  device: %X class: %X  bar : 0x%"PRIx64"  mmio: %p\n",
            id->vendor, id->device, id->class, dev->bar[0].addr, base_mmio);

    pci_irq_alloc(dev, PCI_INT_LEGACY, 1);
    pci_irq_install(dev->legacy_irq, serial_handler, base_mmio, "pci serial 16650");
    pci_irq_umask(dev, dev->legacy_irq);

    /* enable RX interrupts */
    writeb(1, base_mmio + 1);
    return 0;
}

static const struct pci_device_id pci_ids[] = {
    { PCI_VDEVICE(REDHAT, 2) },
    { PCI_DEVICE_CLASS(PCI_CLASS_COMMUNICATION_SERIAL << 8, 0xFFFF00) },
    { /* end: all zeroes */  }
};

/* pci driver glue; this is a "new style" PCI driver module */
static struct pci_driver qemu_16650_pci_driver = {
    .name     = "QEMU-serial",
    .id_table = pci_ids,
    .probe    = serial_probe,
    .remove   = NULL,
};
PCI_DEV_DRIVER_EXPORT(qemu_16650_pci_driver);


#include <shell.h>
static void serial_pci(int argc, char *argv[])
{
    char string[] = "hello pci serial 16650\n";
    char *str = string;

    while (*str)
    {
        writeb(*str, base_mmio);
        str++;
    }

}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)
                | SHELL_CMD_TYPE (SHELL_TYPE_CMD_MAIN)
                | SHELL_CMD_DISABLE_RETURN,
                serial_pci, serial_pci, serial_pci);
