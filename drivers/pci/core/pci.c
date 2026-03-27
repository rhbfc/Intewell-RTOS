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

#include <driver/device.h>
#include <driver/pci/pci_host.h>
#include <io.h>
#include <stdio.h>
#include <ttosMM.h>
#include <ttos_init.h>

#undef KLOG_LEVEL
#define KLOG_TAG "pci-host"
#define KLOG_LEVEL KLOG_DEBUG
#include <klog.h>

#define PCI_BASE_USED 0x80
#define PCI_BASE_ADDRESS(index) (PCI_BASE_ADDRESS_0 + (index)*4)

static int busno = 0;

uint32_t pci_cap_find(struct pci_dev *device, uint32_t cap_id)
{
    uint32_t offset = PCI_CAPABILITY_LIST;
    uint32_t id;

    offset = pci_dev_cfg_readb(device, offset);
    while (offset)
    {
        id = pci_dev_cfg_readb(device, offset + PCI_CAP_LIST_ID);
        if (id == cap_id)
        {
            return offset;
        }
        offset = pci_dev_cfg_readb(device, offset + PCI_CAP_LIST_NEXT);
    }

    return 0;
}

/**
 * @brief 映射 PCI 设备的 BAR 空间
 *
 * 使用托管资源自动管理，驱动卸载时自动释放
 *
 * @param device PCI 设备
 * @param index BAR 索引 (0-5)
 * @return 映射的虚拟地址，失败返回 NULL
 */
void *pci_bar_ioremap(struct pci_dev *device, uint32_t index)
{
    uint64_t address;
    volatile void *mmio_base;

    if ((index > 5) || (~device->bar[index].type & PCI_BASE_USED) || !device->bar[index].size)
    {
        return NULL;
    }

    if (device->bar[index].type & PCI_BASE_ADDRESS_SPACE_IO)
    {
        address = device->bar[index].addr - device->host->io.pci.addr + device->host->io.cpu.addr;
    }
    else if (device->bar[index].type & PCI_BASE_ADDRESS_MEM_PREFETCH)
    {
        address = device->bar[index].addr - device->host->pre_mem.pci.addr +
                  device->host->pre_mem.cpu.addr;
    }
    else
    {
        address = device->bar[index].addr - device->host->mem.pci.addr + device->host->mem.cpu.addr;
    }

    /* 使用托管 I/O 映射，自动释放 */
    mmio_base = devm_ioremap(&device->dev, address, device->bar[index].size);
    return (void *)mmio_base;
}

static inline int pci_dev_is_invalid(struct pci_dev *device)
{
    return (device->vendor == 0) || (device->vendor == 0xFFFF) || (device->device == 0) ||
           (device->device == 0xFFFF);
}

static uint64_t pci_io_mem_alloc(struct pci_iomem *iomem, uint32_t size, uint32_t align)
{
    uint64_t address, offset;

    size = ALIGN_UP(size, align);
    if (size == 0)
    {
        return iomem->pci.addr + iomem->pci.offset;
    }

    offset = ALIGN_UP(iomem->pci.offset, size);
    if (!iomem->pci.size || ((offset + size) > iomem->pci.size))
    {
        return 0;
    }

    address = iomem->pci.addr + offset;
    iomem->pci.offset = offset + size;

    return address;
}

static uint32_t pci_io_alloc(struct pci_dev *device, uint32_t size)
{
    return (uint32_t)pci_io_mem_alloc(&device->host->io, size, 0x1000);
}

static uint64_t pci_mem_alloc(struct pci_dev *device, uint32_t size, uint32_t flag)
{
    struct pci_iomem *mem;

    mem = (flag & PCI_BASE_ADDRESS_MEM_PREFETCH) ? &device->host->pre_mem : &device->host->mem;
    mem = device->host->pre_mem.pci.size ? mem : &device->host->mem;

    return pci_io_mem_alloc(mem, size, 0x100000);
}

static void pci_dev_destroy(struct pci_dev *device)
{
    free(device);
}

static struct pci_dev *pci_dev_create(struct pci_dev *device)
{
    struct pci_dev *dev;

    dev = calloc(1, sizeof(struct pci_dev));

    /* Initialize generic device fields (lists, devres lock, etc.)
     * before any devm_* usage in driver probe paths.
     */
    initialize_device(&dev->dev);

    dev->host = device ? device->host : NULL;

    return dev;
}

static void pci_type1_perconfig(struct pci_dev *device)
{
    uint64_t address;

    if (device->host->flag & PCI_HOST_FLAG_PROBE_ONLY)
        return;

    address = pci_io_alloc(device, 0) >> 8;
    pci_dev_cfg_writeb(device, PCI_IO_BASE, address & 0xF0);
    pci_dev_cfg_writew(device, PCI_IO_BASE_UPPER16, address >> 8);
    pci_dev_cfg_writeb(device, PCI_IO_LIMIT, 0x00);
    pci_dev_cfg_writew(device, PCI_IO_LIMIT_UPPER16, address >> 8);

    address = pci_mem_alloc(device, 0, PCI_BASE_ADDRESS_SPACE_MEMORY) >> 16;
    pci_dev_cfg_writew(device, PCI_MEMORY_BASE, address);
    pci_dev_cfg_writew(device, PCI_MEMORY_LIMIT, 0x00);

    address = pci_mem_alloc(device, 0, PCI_BASE_ADDRESS_MEM_PREFETCH) >> 16;
    pci_dev_cfg_writew(device, PCI_PREF_MEMORY_BASE, address);
    pci_dev_cfg_writel(device, PCI_PREF_BASE_UPPER32, address >> 16);
    pci_dev_cfg_writew(device, PCI_PREF_MEMORY_LIMIT, 0x00);
    pci_dev_cfg_writel(device, PCI_PREF_LIMIT_UPPER32, 0x00 >> 16);
}

static void pci_type1_config(struct pci_dev *device)
{
    uint64_t address, iomem_addr;
    uint32_t type;

    if (device->host->flag & PCI_HOST_FLAG_PROBE_ONLY)
        goto exit;

    pci_dev_cfg_writeb(device, PCI_SUBORDINATE_BUS, device->host->bus_number);

    type = pci_dev_cfg_readb(device, PCI_IO_BASE);
    iomem_addr = (type & PCI_IO_RANGE_MASK) << 8;

    if (type & PCI_IO_RANGE_TYPE_32)
    {
        iomem_addr |= pci_dev_cfg_readw(device, PCI_IO_BASE_UPPER16) << 16;
    }

    address = pci_io_alloc(device, 0);
    if (address != iomem_addr)
    {
        address = (address >> 8) - 1;
        pci_dev_cfg_writeb(device, PCI_IO_LIMIT, address);

        if (type & PCI_IO_RANGE_TYPE_32)
        {
            pci_dev_cfg_writew(device, PCI_IO_LIMIT_UPPER16, address >> 8);
        }
    }

    address = (pci_mem_alloc(device, 0, PCI_BASE_ADDRESS_SPACE_MEMORY) >> 16) - 1;
    pci_dev_cfg_writew(device, PCI_MEMORY_LIMIT, address);

    type = pci_dev_cfg_readw(device, PCI_PREF_MEMORY_BASE);
    address = pci_mem_alloc(device, 0, PCI_BASE_ADDRESS_MEM_PREFETCH);
    iomem_addr = (type & PCI_MEMORY_RANGE_MASK) << 16;
    iomem_addr |= ((uint64_t)pci_dev_cfg_readl(device, PCI_PREF_BASE_UPPER32)) << 32;

    if (address != iomem_addr)
    {
        address = (address >> 16) - 1;
        pci_dev_cfg_writew(device, PCI_PREF_MEMORY_LIMIT, address);

        if (type & PCI_PREF_RANGE_TYPE_64)
        {
            pci_dev_cfg_writel(device, PCI_PREF_LIMIT_UPPER32, address >> 16);
        }
    }

exit:
    pci_dev_cfg_writew(device, PCI_COMMAND,
                       PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER);

    pci_register_device(device);
}

static void pci_dev_type0_probe(struct pci_dev *device)
{
    uint64_t value;
    struct pci_region *bar_n;

    for (int index = 0; index < PCI_STD_NUM_BARS; index++)
    {
        bar_n = &device->bar[index];

        value = pci_dev_cfg_readl(device, PCI_BASE_ADDRESS(index));
        if (value == 0)
            continue;

        bar_n->addr = value & ((value & PCI_BASE_ADDRESS_SPACE_IO) ? PCI_BASE_ADDRESS_IO_MASK
                                                                   : PCI_BASE_ADDRESS_MEM_MASK);
        bar_n->type = value & ((value & PCI_BASE_ADDRESS_SPACE_IO) ? ~PCI_BASE_ADDRESS_IO_MASK
                                                                   : ~PCI_BASE_ADDRESS_MEM_MASK);

        pci_dev_cfg_writel(device, PCI_BASE_ADDRESS(index), (-1U));
        value = pci_dev_cfg_readl(device, PCI_BASE_ADDRESS(index));
        pci_dev_cfg_writel(device, PCI_BASE_ADDRESS(index), bar_n->addr);
        value &= (value & PCI_BASE_ADDRESS_SPACE_IO) ? PCI_BASE_ADDRESS_IO_MASK
                                                     : PCI_BASE_ADDRESS_MEM_MASK;

        if (bar_n->type & PCI_BASE_ADDRESS_MEM_TYPE_64)
        {
            index++;
            bar_n->addr |= ((uint64_t)pci_dev_cfg_readl(device, PCI_BASE_ADDRESS(index)) << 32);
            pci_dev_cfg_writel(device, PCI_BASE_ADDRESS(index), (-1U));
            value |= ((uint64_t)pci_dev_cfg_readl(device, PCI_BASE_ADDRESS(index)) << 32);
            pci_dev_cfg_writel(device, PCI_BASE_ADDRESS(index), bar_n->addr >> 32);
        }
        else
        {
            value |= 0xFFFFFFFF00000000ull;
        }

        bar_n->size = ~value + 1;
        bar_n->type |= PCI_BASE_USED;
        // printk("bar[%d]  addr: %llx  size: %llx\n", index, bar_n->addr, bar_n->size);
    }
}

void pci_dev_type0_config(struct pci_dev *device)
{
    uint64_t value;
    struct pci_region *bar_n;

    if (device->host->flag & PCI_HOST_FLAG_PROBE_ONLY)
    {
        pci_dev_type0_probe(device);
        goto exit;
    }

    for (int index = 0; index < PCI_STD_NUM_BARS; index++)
    {
        bar_n = &device->bar[index];

        pci_dev_cfg_writel(device, PCI_BASE_ADDRESS(index), (-1U));
        value = pci_dev_cfg_readl(device, PCI_BASE_ADDRESS(index));

        if (value == 0)
            continue;

        if (value & PCI_BASE_ADDRESS_SPACE_IO)
        {
            bar_n->type = value & 0x3;
            value = ~(value & PCI_BASE_ADDRESS_IO_MASK) + 1;
            bar_n->size = value & 0xFFFFFFFFull;
            bar_n->addr = pci_io_alloc(device, bar_n->size);
            pci_dev_cfg_writel(device, PCI_BASE_ADDRESS(index), bar_n->addr);
        }
        else
        {
            bar_n->type = value & 0xF;
            value &= PCI_BASE_ADDRESS_MEM_MASK;

            if (bar_n->type & PCI_BASE_ADDRESS_MEM_TYPE_64)
            {
                pci_dev_cfg_writel(device, PCI_BASE_ADDRESS(index + 1), (-1U));
                value |= ((uint64_t)pci_dev_cfg_readl(device, PCI_BASE_ADDRESS(index + 1)) << 32);
            }
            else
            {
                value |= 0xFFFFFFFF00000000ull;
            }

            bar_n->size = ~(value) + 1;
            bar_n->addr = pci_mem_alloc(device, bar_n->size, bar_n->type);

            pci_dev_cfg_writel(device, PCI_BASE_ADDRESS(index), bar_n->addr);
            if (bar_n->type & PCI_BASE_ADDRESS_MEM_TYPE_64)
            {
                index++;
                pci_dev_cfg_writel(device, PCI_BASE_ADDRESS(index), bar_n->addr >> 32);
            }
        }

        bar_n->type |= PCI_BASE_USED;
    }

exit:
    pci_dev_cfg_writew(device, PCI_COMMAND,
                       PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER);

    pci_register_device(device);
}
static int pci_bus_scan(struct pci_dev *device);
static void pci_bridge_dev_create(struct pci_dev *device)
{
    struct pci_dev *dev;
    uint8_t secondary_bus;
    uint8_t subordinate_bus;

    pci_type1_perconfig(device);

    if (!(device->host->flag & PCI_HOST_FLAG_PROBE_ONLY))
    {
        pci_dev_cfg_writeb(device, PCI_PRIMARY_BUS, PCI_BUS(device->bdf));
        pci_dev_cfg_writeb(device, PCI_SECONDARY_BUS, device->host->bus_number + 1 - busno);
        pci_dev_cfg_writeb(device, PCI_SUBORDINATE_BUS, device->host->bus_number + 1 - busno);
    }
    else
    {
        secondary_bus = pci_dev_cfg_readb(device, PCI_SECONDARY_BUS);
        subordinate_bus = pci_dev_cfg_readb(device, PCI_SUBORDINATE_BUS);
    }

    for (int i = secondary_bus; i <= subordinate_bus; i++)
    {
        device->host->bus_number++;
        dev = pci_dev_create(device);
        dev->bdf = PCI_BDF(device->host->bus_number - busno, 0, 0);
        dev->dev.parent = &device->dev;

        pci_bus_scan(dev);
    }
}

static int pci_bus_scan(struct pci_dev *device)
{
    uint32_t type;
    uint32_t bus, dev, func;
    struct pci_dev *secondary_dev;

    bus = PCI_BUS(device->bdf);

    for (dev = 0; dev <= 0x1F; dev++)
    {
        for (func = 0; func <= 0x7; func++)
        {
            device->bdf = PCI_BDF(bus, dev, func);

            device->vendor = pci_dev_cfg_readw(device, PCI_VENDOR_ID);
            device->device = pci_dev_cfg_readw(device, PCI_DEVICE_ID);

            if (pci_dev_is_invalid(device))
            {
                continue;
            }

            device->class = pci_dev_cfg_readl(device, PCI_CLASS_REVISION) >> 8;
            type = pci_dev_cfg_readb(device, PCI_HEADER_TYPE);
            device->is_bridge = type & PCI_HEADER_TYPE_BRIDGE;

            KLOG_D("%d:%d:%d  type: 0x%X  vendor: 0x%X  device: 0x%X  class: 0x%X",
                   PCI_BUS(device->bdf), PCI_DEV(device->bdf), PCI_FUN(device->bdf), type,
                   device->vendor, device->device, device->class);
            if (device->is_bridge)
            {
                pci_type1_config(device);
                pci_bridge_dev_create(device);
            }
            else
            {
                device->subsystem_vendor = pci_dev_cfg_readw(device, PCI_SUBSYSTEM_VENDOR_ID);
                device->subsystem_device = pci_dev_cfg_readw(device, PCI_SUBSYSTEM_ID);
                pci_dev_type0_config(device);
            }

            device = pci_dev_create(device);
            if ((func == 0) && !(type & PCI_HEADER_TYPE_MFD))
            {
                break;
            }
        }
    }

    if (pci_dev_is_invalid(device))
    {
        pci_dev_destroy(device);
    }

    return 0;
}

static int pci_root_scan(struct pci_host *host)
{
    struct pci_dev *device = pci_dev_create(NULL);

    device->bdf = PCI_BDF(host->bus_number - busno, 0, 0);
    device->host = host;

    pci_bus_scan(device);
    return 0;
}
extern void pci_dbg_host_set(struct pci_host *host);
int pci_host_register(struct pci_host *host)
{
    initialize_device(&host->dev);
    host->dev.bus = &pci_bus_type;
    register_device(&host->dev);

    pci_dbg_host_set(host);

#ifdef CONFIG_ACPI
    pci_host_pnp_parmes(host);
#else
    if (!(host->flag & PCI_HOST_FLAG_SPIK_DT))
    {
        pci_host_dt_parmes(host);
    }
#endif

    host->bus_number = busno;
    pci_root_scan(host);
    busno = host->bus_number;

    return 0;
}

#define PCI_byte_BAD 0
#define PCI_word_BAD ((where)&1)
#define PCI_dword_BAD ((where)&3)

#define PCI_BUS_READ_CONFIG(len, type, size)                                                       \
    int pci_bus_read_config_##len(struct pci_host *bus, unsigned int devfn, int where,             \
                                  type *value)                                                     \
    {                                                                                              \
        int ret = -EINVAL;                                                                         \
        uint32_t data = 0;                                                                         \
                                                                                                   \
        if (!PCI_##len##_BAD)                                                                      \
        {                                                                                          \
            ret = pci_bus_read_config(bus, devfn, where, size, &data);                             \
        }                                                                                          \
                                                                                                   \
        *value = (type)ret;                                                                        \
        return ret;                                                                                \
    }

#define PCI_BUS_WRITE_CONFIG(len, type, size)                                                      \
    int pci_bus_write_config_##len(struct pci_host *bus, unsigned int devfn, int where,            \
                                   type value)                                                     \
    {                                                                                              \
        int ret = -EINVAL;                                                                         \
                                                                                                   \
        if (!PCI_##len##_BAD)                                                                      \
        {                                                                                          \
            ret = pci_bus_write_config(bus, devfn, where, size, value);                            \
        }                                                                                          \
                                                                                                   \
        return ret;                                                                                \
    }

#define PCI_BUS_READ_IO(len, type, size)                                                           \
    int pci_bus_read_io_##len(struct pci_host *bus, uintptr_t where, type *value)                  \
    {                                                                                              \
        int ret = -EINVAL;                                                                         \
        uint32_t data = 0;                                                                         \
                                                                                                   \
        if (!PCI_##len##_BAD)                                                                      \
        {                                                                                          \
            ret = pci_bus_read_io(bus, where, size, &data);                                        \
        }                                                                                          \
                                                                                                   \
        *value = (type)data;                                                                       \
        return ret;                                                                                \
    }

#define PCI_BUS_WRITE_IO(len, type, size)                                                          \
    int pci_bus_write_io_##len(struct pci_host *bus, uintptr_t where, type value)                  \
    {                                                                                              \
        int ret = -EINVAL;                                                                         \
                                                                                                   \
        if (!PCI_##len##_BAD)                                                                      \
        {                                                                                          \
            ret = pci_bus_write_io(bus, where, size, value);                                       \
        }                                                                                          \
                                                                                                   \
        return ret;                                                                                \
    }

/****************************************************************************
 * Name: pci_bus_read_config
 *
 * Description:
 *  Read pci device config space
 *
 * Input Parameters:
 *   bus   - The PCI device belong to
 *   devfn - The PCI device dev number and function number
 *   where - The register address
 *   size  - The data length
 *   val   - The data buffer
 *
 * Returned Value:
 *   Zero if success, otherwise nagative
 *
 ****************************************************************************/

int pci_bus_read_config(struct pci_host *bus, unsigned int devfn, int where, int size,
                        uint32_t *val)
{
    if (size != 1 && size != 2 && size != 4)
    {
        return -EINVAL;
    }

    return bus->ops->cfg_rw(bus, devfn, where, *val, size, false);
}

/****************************************************************************
 * Name: pci_bus_write_config
 *
 * Description:
 *  Read pci device config space
 *
 * Input Parameters:
 *   bus   - The PCI device belong to
 *   devfn - The PCI device dev number and function number
 *   where - The register address
 *   size  - The data length
 *   val   - The data
 *
 * Returned Value:
 *   Zero if success, otherwise nagative
 *
 ****************************************************************************/

int pci_bus_write_config(struct pci_host *bus, unsigned int devfn, int where, int size,
                         uint32_t val)
{
    if (size != 1 && size != 2 && size != 4)
    {
        return -EINVAL;
    }

    return bus->ops->cfg_rw(bus, devfn, where, val, size, true);
}

/****************************************************************************
 * Name: pci_bus_read_io
 *
 * Description:
 *  Read pci device io space
 *
 * Input Parameters:
 *   bus   - The PCI device belong to
 *   addr  - The address to read
 *   size  - The data length
 *   val   - The data buffer
 *
 * Returned Value:
 *   Zero if success, otherwise nagative
 *
 ****************************************************************************/

int pci_bus_read_io(struct pci_host *bus, uintptr_t addr, int size, uint32_t *val)
{
    if (size != 1 && size != 2 && size != 4)
    {
        return -EINVAL;
    }

    return bus->ops->read_io(bus, addr, size, val);
}

/****************************************************************************
 * Name: pci_bus_write_io
 *
 * Description:
 *  Read pci device io space
 *
 * Input Parameters:
 *   bus   - The PCI device belong to
 *   addr  - The address to write
 *   size  - The data length
 *   val   - The data
 *
 * Returned Value:
 *   Zero if success, otherwise nagative
 *
 ****************************************************************************/

int pci_bus_write_io(struct pci_host *bus, uintptr_t addr, int size, uint32_t val)
{
    if (size != 1 && size != 2 && size != 4)
    {
        return -EINVAL;
    }

    return bus->ops->write_io(bus, addr, size, val);
}

/****************************************************************************
 * Name: pci_bus_read_config_xxx
 *
 * Description:
 *  Read pci device config space
 *
 * Input Parameters:
 *   bus   - The PCI device to belong to
 *   devfn - The PCI device number and function number
 *   where - The register address
 *   val   - The data buf
 *
 * Returned Value:
 *   Zero if success, otherwise nagative
 *
 ****************************************************************************/

PCI_BUS_READ_CONFIG(byte, uint8_t, 1)
PCI_BUS_READ_CONFIG(word, uint16_t, 2)
PCI_BUS_READ_CONFIG(dword, uint32_t, 4)

/****************************************************************************
 * Name: pci_bus_write_config_xxx
 *
 * Description:
 *  Write pci device config space
 *
 * Input Parameters:
 *   bus   - The PCI device to belong to
 *   devfn - The PCI device number and function number
 *   where - The register address
 *   val   - The data
 *
 * Returned Value:
 *   Zero if success, otherwise nagative
 *
 ****************************************************************************/

PCI_BUS_WRITE_CONFIG(byte, uint8_t, 1)
PCI_BUS_WRITE_CONFIG(word, uint16_t, 2)
PCI_BUS_WRITE_CONFIG(dword, uint32_t, 4)

/****************************************************************************
 * Name: pci_bus_read_io_xxx
 *
 * Description:
 *  Read pci device io space
 *
 * Input Parameters:
 *   bus   - The PCI device belong to
 *   where - The address to read
 *   val   - The data buffer
 *
 * Returned Value:
 *   Zero if success, otherwise nagative
 *
 ****************************************************************************/

PCI_BUS_READ_IO(byte, uint8_t, 1)
PCI_BUS_READ_IO(word, uint16_t, 2)
PCI_BUS_READ_IO(dword, uint32_t, 4)

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

PCI_BUS_WRITE_IO(byte, uint8_t, 1)
PCI_BUS_WRITE_IO(word, uint16_t, 2)
PCI_BUS_WRITE_IO(dword, uint32_t, 4)
