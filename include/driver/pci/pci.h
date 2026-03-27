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

#ifndef __INCLUDE_PCI_H__
#define __INCLUDE_PCI_H__

#include <driver/devbus.h>
#include <driver/device.h>
#include <driver/driver.h>
#include <driver/pci/pci_ids.h>
#include <driver/pci/pci_regs.h>
#include <inttypes.h>
#include <stdint.h>
#include <ttos_init.h>

#define PCI_ANY_ID (~0)

#define PCI_BDF(bus, dev, func) (((bus) << 8) | ((dev) << 3) | (func))
#define PCI_BUS(bdf) (((bdf) >> 8) & 0xFF)
#define PCI_DEVFN(bdf) ((bdf)&0xFF)
#define PCI_DEV(bdf) (((bdf) >> 3) & 0x1F)
#define PCI_FUN(bdf) ((bdf)&0x7)
#define PCI_CLASS(class) ((class) >> 8)
#define PCI_REV(class) ((class) & 0xFF)

#define PCI_RESOURCE_IO 0x00000001
#define PCI_RESOURCE_MEM 0x00000002
#define PCI_RESOURCE_MEM_64 0x00000004
#define PCI_RESOURCE_PREFETCH 0x00000008 /* No side effects */

typedef void (*pci_isr_handler_t)(uint32_t irq, void *param);

struct pci_device_id;
struct driver;
struct pci_driver;
struct pci_host;

struct pci_region
{
    uint64_t addr;
    uint64_t size;

    union {
        uint64_t offset;
        uint64_t type;
    };
};

struct pci_iomem
{
    struct pci_region pci;
    struct pci_region cpu;
};

struct pci_msg_int
{
    uint64_t addr;
    uint32_t data;
    uint32_t irq;
};

struct pci_msix
{
    uint64_t table_address;
    uint64_t pab_address;
};

enum PCI_IRQ_TYPE
{
    PCI_INT_NULL = 0,
    PCI_INT_LEGACY,
    PCI_INT_MSI,
    PCI_INT_MSIX,
    PCI_INT_AUTO,
};

struct pci_dev
{
    struct device dev;

    /* device data */
    uint32_t bdf; /* Encoded device & function index */
    uint16_t vendor;
    uint16_t device;
    uint16_t subsystem_vendor;
    uint16_t subsystem_device;
    uint32_t class; /* 3 bytes: (base,sub,prog-if) */
    uint32_t legacy_irq;
    uint32_t irq_type;
    uint32_t msi_count;
    struct pci_msix msix;
    struct pci_msg_int *msi;
    struct pci_driver *driver; /* Driver bound to this device */
    struct list_head children;
    struct list_head devices;

    /* common data */
    struct pci_region bar[6];
    struct pci_host *host;
    struct list_head list;
    bool is_bridge;
    void *priv;
};

struct pci_driver
{
    const char *name;
    const struct pci_device_id *id_table;
    struct driver driver;
    int (*probe)(struct pci_dev *dev, const struct pci_device_id *id); /* New device inserted */
    void (*remove)(struct pci_dev *dev); /* Device removed (NULL if not a
                                            hot-plug capable driver) */
};

/* pci_dt.c */
int pci_host_dt_parmes(struct pci_host *host);

/* pci_pnp.c */
int pci_host_pnp_parmes(struct pci_host *host);

/* pci_bus.c */
int pci_register_device(struct pci_dev *device);
int pci_register_driver(struct pci_driver *driver);

/* pci_irq.c */
int pci_irq_alloc(struct pci_dev *dev, uint32_t type, uint32_t max_count);
int pci_irq_install(uint32_t irq, pci_isr_handler_t handler, void *arg, const char *name);
void pci_irq_mask(struct pci_dev *dev, uint32_t irq);
void pci_irq_umask(struct pci_dev *dev, uint32_t irq);

/* pci.c */
void *pci_bar_ioremap(struct pci_dev *device, uint32_t index);
uint32_t pci_cap_find(struct pci_dev *device, uint32_t cap_id);

#define PCI_DEVICE(vend, dev)                                                                      \
    .vendor = (vend), .device = (dev), .subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID

#define PCI_DEVICE_SUB(vend, dev, subvend, subdev)                                                 \
    .vendor = (vend), .device = (dev), .subvendor = (subvend), .subdevice = (subdev)

#define PCI_DEVICE_CLASS(dev_class, dev_class_mask)                                                \
    .class = (dev_class), .class_mask = (dev_class_mask), .vendor = PCI_ANY_ID,                    \
    .device = PCI_ANY_ID, .subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID

#define PCI_VDEVICE(vend, dev)                                                                     \
    .vendor = PCI_VENDOR_ID_##vend, .device = (dev), .subvendor = PCI_ANY_ID,                      \
    .subdevice = PCI_ANY_ID, 0, 0

#define PCI_DEVICE_DATA(vend, dev, data)                                                           \
    .vendor = PCI_VENDOR_ID_##vend, .device = PCI_DEVICE_ID_##vend##_##dev,                        \
    .subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID, 0, 0, .driver_data = (void *)(data)

#define pci_dev_print(dev, fmt, arg...)                                                            \
    printk("%" PRId32 ":%" PRId32 ":%" PRId32 "   " fmt, PCI_BUS(dev->bdf), PCI_DEV(dev->bdf),     \
           PCI_FUN(dev->bdf), arg);

#define PCI_HOST_DRIVER_EXPORT(driver)                                                             \
    static int __pci_host_driver_##driver##_register(void)                                         \
    {                                                                                              \
        return platform_add_driver(&driver);                                                       \
    }                                                                                              \
    INIT_EXPORT_PRE_DRIVER(__pci_host_driver_##driver##_register,                                  \
                           "pci host driver " #driver " register");

#define PCI_DEV_DRIVER_EXPORT(driver)                                                              \
    static int __pci_device_driver_##driver##_register(void)                                       \
    {                                                                                              \
        return pci_register_driver(&driver);                                                       \
    }                                                                                              \
    INIT_EXPORT_DRIVER(__pci_device_driver_##driver##_register,                                    \
                       "pci device driver " #driver " register");

#define pci_resource_size(res) ((res)->size)

#define pci_resource_start(dev, bar) ((dev)->bar[(bar)].addr)
#define pci_resource_end(dev, bar) ((dev)->bar[(bar)].addr + (dev)->bar[(bar)].size - 1)
#define pci_resource_flags(dev, bar) ((dev)->bar[(bar)].type)
#define pci_resource_len(dev, bar) pci_resource_size(&((dev)->bar[(bar)]))

/****************************************************************************
 * Name: pci_bus_read_config_byte
 *
 * Description:
 *   Read data from specify position with byte size
 *
 * Input Parameters:
 *   bus   - PCI bus
 *   devfn - BDF
 *   where - Pos ID
 *   val   - Value
 *
 * Return value
 *   Return 0 if success, otherwise Error values
 *
 ****************************************************************************/

int pci_bus_read_config_byte(struct pci_host *bus, unsigned int devfn, int where, uint8_t *val);

/****************************************************************************
 * Name: pci_bus_read_config_word
 *
 * Description:
 *   Read data from specify position with word size
 *
 * Input Parameters:
 *   bus   - PCI bus
 *   devfn - BDF
 *   where - Pos ID
 *   val   - Value
 *
 * Return value
 *   Return 0 if success, otherwise Error values
 *
 ****************************************************************************/

int pci_bus_read_config_word(struct pci_host *bus, unsigned int devfn, int where, uint16_t *val);

/****************************************************************************
 * Name: pci_bus_read_config_dword
 *
 * Description:
 *   Read data from specify position with dword size
 *
 * Input Parameters:
 *   bus   - PCI bus
 *   devfn - BDF
 *   where - Pos ID
 *   val   - Value
 *
 * Return value
 *   Return 0 if success, otherwise Error values
 *
 ****************************************************************************/

int pci_bus_read_config_dword(struct pci_host *bus, unsigned int devfn, int where, uint32_t *val);

/****************************************************************************
 * Name: pci_bus_write_config_byte
 *
 * Description:
 *   Write data to specify reg with byte size
 *
 * Input Parameters:
 *   bus   - PCI bus
 *   devfn - BDF
 *   where - Pos ID
 *   val   - Value
 *
 * Return value
 *   Return 0 if success, otherwise Error values
 *
 ****************************************************************************/

int pci_bus_write_config_byte(struct pci_host *bus, unsigned int devfn, int where, uint8_t val);

/****************************************************************************
 * Name: pci_bus_write_config_word
 *
 * Description:
 *   Write data to specify reg with word size
 *
 * Input Parameters:
 *   bus   - PCI bus
 *   devfn - BDF
 *   where - Pos ID
 *   val   - Value
 *
 * Return value
 *   Return 0 if success, otherwise Error values
 *
 ****************************************************************************/

int pci_bus_write_config_word(struct pci_host *bus, unsigned int devfn, int where, uint16_t val);

/****************************************************************************
 * Name: pci_bus_write_config_dword
 *
 * Description:
 *   Write data to specify reg with dword size
 *
 * Input Parameters:
 *   bus   - PCI bus
 *   devfn - BDF
 *   where - Pos ID
 *   val   - Value
 *
 * Return value
 *   Return 0 if success, otherwise Error values
 *
 ****************************************************************************/

int pci_bus_write_config_dword(struct pci_host *bus, unsigned int devfn, int where, uint32_t val);

int pci_bus_read_io_byte(struct pci_host *bus, uintptr_t where, uint8_t *val);
int pci_bus_read_io_word(struct pci_host *bus, uintptr_t where, uint16_t *val);
int pci_bus_read_io_dword(struct pci_host *bus, uintptr_t where, uint32_t *val);

/****************************************************************************
 * Name: pci_bus_write_io_xxx
 *
 * Description:
 *  Write pci device io space
 *
 * Input Parameters:
 *   bus   - The PCI device belong to
 *   where - The address to write
 *   val   - The data
 *
 * Returned Value:
 *   Zero if success, otherwise nagative
 *
 ****************************************************************************/

int pci_bus_write_io_byte(struct pci_host *bus, uintptr_t where, uint8_t value);
int pci_bus_write_io_word(struct pci_host *bus, uintptr_t where, uint16_t value);
int pci_bus_write_io_dword(struct pci_host *bus, uintptr_t where, uint32_t value);

#define pci_read_config_byte(dev, where, val)                                                      \
    pci_bus_read_config_byte(dev->host, (dev)->bdf, where, val)

#define pci_read_config_word(dev, where, val)                                                      \
    pci_bus_read_config_word(dev->host, (dev)->bdf, where, val)

#define pci_read_config_dword(dev, where, val)                                                     \
    pci_bus_read_config_dword(dev->host, (dev)->bdf, where, val)

#define pci_write_config_byte(dev, where, val)                                                     \
    pci_bus_write_config_byte(dev->host, (dev)->bdf, where, val)

#define pci_write_config_word(dev, where, val)                                                     \
    pci_bus_write_config_word(dev->host, (dev)->bdf, where, val)

#define pci_write_config_dword(dev, where, val)                                                    \
    pci_bus_write_config_dword(dev->host, (dev)->bdf, where, val)

#define pci_read_io_byte(dev, addr, val) pci_bus_read_io_byte(dev->host, addr, val)

#define pci_read_io_word(dev, addr, val) pci_bus_read_io_word(dev->host, addr, val)

#define pci_read_io_dword(dev, addr, val) pci_bus_read_io_dword(dev->host, addr, val)

#define pci_read_io_qword(dev, addr, val)                                                          \
    do                                                                                             \
    {                                                                                              \
        uint32_t valhi;                                                                            \
        uint32_t vallo;                                                                            \
        pci_bus_read_io_dword(dev->host, addr, &vallo);                                            \
        pci_bus_read_io_dword(dev->host, (uintptr_t)(addr) + sizeof(uint32_t), &valhi);            \
        *(val) = ((uint64_t)valhi << 32) | (uint64_t)vallo;                                        \
    } while (0)

#define pci_write_io_byte(dev, addr, val) pci_bus_write_io_byte(dev->host, addr, val)

#define pci_write_io_word(dev, addr, val) pci_bus_write_io_word(dev->host, addr, val)

#define pci_write_io_dword(dev, addr, val) pci_bus_write_io_dword(dev->host, addr, val)

#define pci_write_io_qword(dev, addr, val)                                                         \
    do                                                                                             \
    {                                                                                              \
        pci_bus_write_io_dword(dev->host, addr, (uint32_t)(val));                                  \
        pci_bus_write_io_dword(dev->host, (uintptr_t)(addr) + sizeof(uint32_t), (val) >> 32);      \
    } while (0)

#endif /* __INCLUDE_PCI_H__ */