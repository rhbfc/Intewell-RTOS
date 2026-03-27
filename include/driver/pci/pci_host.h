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

#include <driver/of.h>
#include <driver/pci/pci.h>

#ifndef __INCLUDE_PCI_HOST_H__
#define __INCLUDE_PCI_HOST_H__

#define PCI_HOST_FLAG_SPIK_DT (1 << 0)
#define PCI_HOST_FLAG_FORCE_MEM64 (1 << 1)
#define PCI_HOST_FLAG_SUPPORT_LEGACY_INT (1 << 2)

#define PCI_HOST_FLAG_PROBE_ONLY (1 << 8)

struct pci_host;
struct pci_region;
struct pci_iomem;
extern struct bus_type pci_bus_type;

struct pci_bus_ranges
{
    uint32_t start;
    uint32_t end;
};

struct pci_ops
{
    uint32_t (*cfg_rw)(struct pci_host *host, uint32_t bdf, uint32_t offset, uint32_t value,
                       uint32_t bytes, bool is_write);

    int (*read_io)(struct pci_host *bus, uintptr_t addr, int size, uint32_t *val);
    int (*write_io)(struct pci_host *bus, uintptr_t addr, int size, uint32_t val);

    struct pci_msg_int *(*msi_alloc)(struct pci_host *host, uint32_t bdf);
};

struct msi_map
{
    uint32_t input_base;
    void *pic;
    uint32_t output_base;
    uint32_t count;
};
struct pci_host
{
    struct device dev;
    const struct pci_ops *ops;
    struct pci_bus_ranges bus_ranges;
    struct devaddr_region cfg;
    struct pci_iomem io;
    struct pci_iomem mem;
    struct pci_iomem pre_mem;
    uint32_t flag;
    uint32_t bus_number;
    uint32_t legacy_irq[4];
    void *pic;
    uint32_t msi_map_count;
    struct msi_map *msi_map;
};

static inline uint32_t _pci_cfg_read(struct pci_host *host, uint32_t bdf, uint32_t offset,
                                     uint32_t bytes)
{
    return host->ops->cfg_rw(host, bdf, offset, 0, bytes, false);
}

static inline uint32_t _pci_cfg_write(struct pci_host *host, uint32_t bdf, uint32_t offset,
                                      uint32_t value, uint32_t bytes)
{
    return host->ops->cfg_rw(host, bdf, offset, value, bytes, true);
}

int pci_host_register(struct pci_host *host);

// clang-format off
#define pci_cfg_readb(host, bdf, offset)        _pci_cfg_read(host, bdf, offset, 1)
#define pci_cfg_readw(host, bdf, offset)        _pci_cfg_read(host, bdf, offset, 2)
#define pci_cfg_readl(host, bdf, offset)        _pci_cfg_read(host, bdf, offset, 4)

#define pci_dev_cfg_readb(dev, offset)          _pci_cfg_read (dev->host, dev->bdf, offset, 1)
#define pci_dev_cfg_readw(dev, offset)          _pci_cfg_read (dev->host, dev->bdf, offset, 2)
#define pci_dev_cfg_readl(dev, offset)          _pci_cfg_read (dev->host, dev->bdf, offset, 4)
#define pci_dev_cfg_writeb(dev, offset, value)  _pci_cfg_write(dev->host, dev->bdf, offset, value, 1)
#define pci_dev_cfg_writew(dev, offset, value)  _pci_cfg_write(dev->host, dev->bdf, offset, value, 2)
#define pci_dev_cfg_writel(dev, offset, value)  _pci_cfg_write(dev->host, dev->bdf, offset, value, 4)
// clang-format on

#endif /* __INCLUDE_PCI_HOST_H__ */