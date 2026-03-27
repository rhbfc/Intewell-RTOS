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

#include "barrier.h"
#include <driver/pci/pci_host.h>
#include <ttos_pic.h>
#include <io.h>

#define KLOG_TAG "pciirq"
#include <klog.h>

struct msix_entry
{
    uint32_t addr_lo;
    uint32_t addr_hi;
    uint32_t data;
    uint32_t vector_ctrl;
};

static inline uint32_t pci_msi_mapped_dev_id(struct pci_dev *dev, const struct msi_map *map)
{
    if (map)
        return map->output_base + (dev->bdf - map->input_base);
    return dev->bdf;
}

static int pci_msi_config(struct pci_dev *dev)
{
    uint32_t msi_offset;
    uint16_t ctl;

    msi_offset = pci_cap_find(dev, PCI_CAP_ID_MSI);
    if (!msi_offset)
    {
        return -1;
    }

    ctl = pci_dev_cfg_readw(dev, msi_offset + PCI_MSI_FLAGS);

    pci_dev_cfg_writel(dev, msi_offset + PCI_MSI_ADDRESS_LO, dev->msi->addr & 0xFFFFFFFF);
    if (ctl & PCI_MSI_FLAGS_64BIT)
    {
        pci_dev_cfg_writel(dev, msi_offset + PCI_MSI_ADDRESS_HI,
                           (dev->msi->addr >> 32) & 0xFFFFFFFF);

        pci_dev_cfg_writew(dev, msi_offset + PCI_MSI_DATA_64, (dev->msi->data) & 0xFFFF);
    }
    else
    {
        pci_dev_cfg_writew(dev, msi_offset + PCI_MSI_DATA_32, (dev->msi->data) & 0xFFFF);
    }

    ctl &= ~(PCI_MSI_FLAGS_QSIZE | PCI_MSI_FLAGS_ENABLE);
    ctl |= ((31 - __builtin_clz(dev->msi_count)) << 4) & PCI_MSI_FLAGS_QSIZE;
    pci_dev_cfg_writew(dev, msi_offset + PCI_MSI_FLAGS, ctl);

    return 0;
}

static int pci_irq_legacy_alloc(struct pci_dev *dev)
{
    uint32_t int_pin;
    ttos_irq_desc_t desc = NULL;

    int_pin = pci_dev_cfg_readb(dev, PCI_INTERRUPT_PIN);
    if (!int_pin)
    {
        return -1;
    }

    if (!dev->host->legacy_irq[int_pin - 1])
    {
        return -1;
    }

    int_pin -= 1;
    int_pin += PCI_DEV(dev->bdf);
    int_pin &= 0x3;
    dev->legacy_irq = dev->host->legacy_irq[int_pin];

    desc = ttos_pic_irq_desc_get(dev->legacy_irq);
    if (desc)
    {
        desc->pirv = dev;
    }

    return 1;
}

static int pci_irq_msi_alloc(struct pci_dev *dev, uint32_t max_count)
{
    int32_t ret;
    uint32_t i;
    uint32_t msi_offset;
    ttos_irq_desc_t desc;
    struct ttos_pic *pic = NULL;
    struct msi_map *map = NULL;
    uint32_t mapped_id;

    msi_offset = pci_cap_find(dev, PCI_CAP_ID_MSI);
    if (!msi_offset || (max_count == 0))
        return -1;

    max_count = 1U << (32 - __builtin_clz(max_count - 1));
    dev->msi_count =
        1 << ((pci_dev_cfg_readw(dev, msi_offset + PCI_MSI_FLAGS) & PCI_MSI_FLAGS_QMASK) >> 1);
    dev->msi_count = dev->msi_count < max_count ? dev->msi_count : max_count;

    if (dev->host->pic)
    {
        pic = (struct ttos_pic *)dev->host->pic;
    }
    else
    {
        for (i = 0; i < dev->host->msi_map_count; i++)
        {
            if (dev->bdf >= dev->host->msi_map[i].input_base &&
                dev->bdf < (dev->host->msi_map[i].input_base + dev->host->msi_map[i].count))
            {
                pic = (struct ttos_pic *)dev->host->msi_map[i].pic;
                map = &dev->host->msi_map[i];
                break;
            }
        }
    }

    if (!pic)
        return -1;

    dev->msi = malloc(dev->msi_count * sizeof(struct pci_msg_int));

    mapped_id = pci_msi_mapped_dev_id(dev, map);
    ret = ttos_pic_msi_alloc(pic, mapped_id, dev->msi_count);
    if (ret)
    {
        return -1;
    }

    KLOG_I("MSI alloc: count=%u pic=%p mapped_id=0x%x", dev->msi_count, pic, mapped_id);
    for (i = 0; i < dev->msi_count; i++)
    {
        dev->msi[i].irq = ttos_pic_msi_map(pic, mapped_id, i);

        KLOG_D("MSI vec %u: mapped_id=0x%x addr=0x%llx data=0x%08x irq=%u", i, mapped_id,
               (unsigned long long)dev->msi[i].addr, dev->msi[i].data, dev->msi[i].irq);

        desc = ttos_pic_irq_desc_get(dev->msi[i].irq);
        if (desc)
        {
            desc->pirv = dev;
            dev->msi[i].data = ttos_pic_msi_data(pic, i, desc);
            dev->msi[i].addr = ttos_pic_msi_address(pic);
        }
    }

    pci_msi_config(dev);
    return dev->msi_count;
}

static int pci_irq_msix_alloc(struct pci_dev *dev, uint32_t max_count)
{
    int32_t ret;
    void *address;
    uint32_t i, value = 0;
    uint32_t msix_offset;
    struct msix_entry *entry;
    ttos_irq_desc_t desc;

    struct ttos_pic *pic = NULL;
    struct msi_map *msi_map = NULL;

    msix_offset = pci_cap_find(dev, PCI_CAP_ID_MSIX);
    if (!msix_offset || (max_count == 0))
        return -1;

    if (!dev->msix.table_address)
    {
        value = pci_dev_cfg_readl(dev, msix_offset + PCI_MSIX_TABLE);
        address = pci_bar_ioremap(dev, value & PCI_MSIX_TABLE_BIR);
        dev->msix.table_address = value & PCI_MSIX_TABLE_OFFSET;
        dev->msix.table_address += (uint64_t)(uintptr_t)address;
    }

    if (!dev->msix.pab_address)
    {
        value = pci_dev_cfg_readl(dev, msix_offset + PCI_MSIX_PBA);
        address = pci_bar_ioremap(dev, value & PCI_MSIX_PBA_BIR);
        dev->msix.pab_address = value & PCI_MSIX_PBA_OFFSET;
        dev->msix.pab_address += (uint64_t)(uintptr_t)address;
    }

    dev->msi_count =
        (pci_dev_cfg_readw(dev, msix_offset + PCI_MSIX_FLAGS) & PCI_MSIX_FLAGS_QSIZE) + 1;
    dev->msi_count = dev->msi_count < max_count ? dev->msi_count : max_count;

    if (dev->host->pic)
    {
        pic = (struct ttos_pic *)dev->host->pic;
    }
    else
    {
        for (i = 0; i < dev->host->msi_map_count; i++)
        {
            if (dev->bdf >= dev->host->msi_map[i].input_base &&
                dev->bdf < (dev->host->msi_map[i].input_base + dev->host->msi_map[i].count))
            {
                pic = (struct ttos_pic *)dev->host->msi_map[i].pic;
                msi_map = &dev->host->msi_map[i];
                break;
            }
        }
    }

    if (pic)
    {
        uint32_t mapped_id = pci_msi_mapped_dev_id(dev, msi_map);
        dev->msi = malloc(dev->msi_count * sizeof(struct pci_msg_int));

        ret = ttos_pic_msi_alloc((struct ttos_pic *)pic, mapped_id, dev->msi_count);
        if (ret)
        {
            return -1;
        }

        entry = (struct msix_entry *)(uintptr_t)dev->msix.table_address;

        for (i = 0; i < dev->msi_count; i++)
        {
            dev->msi[i].irq = ttos_pic_msi_map((struct ttos_pic *)pic, mapped_id, i);

            desc = ttos_pic_irq_desc_get(dev->msi[i].irq);
            if (desc)
            {
                dev->msi[i].data = ttos_pic_msi_data(pic, i, desc);
                dev->msi[i].addr = ttos_pic_msi_address(pic);

                writel(dev->msi[i].data, &entry[i].data);
                
                writel(dev->msi[i].addr >> 32, &entry[i].addr_hi);
                writel(dev->msi[i].addr & 0xFFFFFFFF, &entry[i].addr_lo);

                writel(PCI_MSIX_ENTRY_CTRL_MASKBIT, &entry[i].vector_ctrl);
                desc->pirv = dev;
            }
        }

        value = pci_dev_cfg_readw(dev, msix_offset + PCI_MSIX_FLAGS);
        value |= PCI_MSIX_FLAGS_ENABLE;
        value &= ~PCI_MSIX_FLAGS_MASKALL; /* ensure global mask-all is cleared */
        pci_dev_cfg_writew(dev, msix_offset + PCI_MSIX_FLAGS, value);
    }

    return dev->msi_count;
}

void pci_irq_umask(struct pci_dev *dev, uint32_t irq)
{
    struct msix_entry *entry;
    uint32_t cfg, cap, i;

    if (!dev)
    {
        return;
    }

    switch (dev->irq_type)
    {
    case PCI_INT_LEGACY:
        cfg = pci_dev_cfg_readw(dev, PCI_COMMAND);
        cfg &= ~PCI_COMMAND_INTX_DISABLE;
        pci_dev_cfg_writew(dev, PCI_COMMAND, cfg);
        break;

    case PCI_INT_MSI:
        cap = pci_cap_find(dev, PCI_CAP_ID_MSI);
        cfg = pci_dev_cfg_readw(dev, cap + PCI_MSI_FLAGS);
        cfg |= PCI_MSI_FLAGS_ENABLE;
        pci_dev_cfg_writew(dev, cap + PCI_MSI_FLAGS, cfg);
        break;

    case PCI_INT_MSIX:
        if (dev->msix.table_address)
        {
            entry = (struct msix_entry *)(uintptr_t)dev->msix.table_address;
            for (i = 0; i < dev->msi_count; i++)
            {
                if (dev->msi[i].irq == irq)
                {
                    entry[i].vector_ctrl = 0;
                    wmb();
                    goto unmask;
                }
            }
        }
        return;
    default:
        break;
    }

unmask:
    ttos_pic_irq_unmask(irq);
}

void pci_irq_mask(struct pci_dev *dev, uint32_t irq)
{
    struct msix_entry *entry;
    uint32_t cfg, cap, i;

    if (!dev)
    {
        return;
    }

    switch (dev->irq_type)
    {
    case PCI_INT_MSI:
        cap = pci_cap_find(dev, PCI_CAP_ID_MSI);
        cfg = pci_dev_cfg_readw(dev, cap + PCI_MSI_FLAGS);
        cfg &= ~PCI_MSI_FLAGS_ENABLE;
        pci_dev_cfg_writew(dev, cap + PCI_MSI_FLAGS, cfg);
        break;

    case PCI_INT_LEGACY:
        cfg = pci_dev_cfg_readw(dev, PCI_COMMAND);
        cfg |= PCI_COMMAND_INTX_DISABLE;
        pci_dev_cfg_writew(dev, PCI_COMMAND, cfg);
        break;

    case PCI_INT_MSIX:
        entry = (struct msix_entry *)(uintptr_t)dev->msix.table_address;
        for (i = 0; i < dev->msi_count; i++)
        {
            if (dev->msi[i].irq == irq)
            {
                entry[i].vector_ctrl = PCI_MSIX_ENTRY_CTRL_MASKBIT;
                goto mask;
            }
        }
        return;
    default:
        break;
    }

mask:
    ttos_pic_irq_mask(irq);
}

/* 返回alloc到的中断个数, -1:失败 */
int pci_irq_alloc(struct pci_dev *dev, uint32_t type, uint32_t max_count)
{
    uint32_t i;
    int ret;

    if (type > PCI_INT_AUTO)
    {
        return -1;
    }

    for (i = type; i > PCI_INT_NULL; i--)
    {
        ret = 0;

        switch (i)
        {
        case PCI_INT_MSIX:
            ret = pci_irq_msix_alloc(dev, max_count);
            break;

        case PCI_INT_MSI:
            ret = pci_irq_msi_alloc(dev, max_count);
            break;

        case PCI_INT_LEGACY:
            ret = pci_irq_legacy_alloc(dev);
            break;

        default:
            break;
        }

        if (ret)
        {
            dev->irq_type = i;
            return ret;
        }

        if (type != PCI_INT_AUTO)
        {
            return -1;
        }
    }

    return -1;
}

/**
 * @brief 安装 PCI 设备中断处理函数
 *
 * 使用托管资源自动管理，驱动卸载时自动卸载中断
 * 支持 MSI、MSI-X 和传统中断
 *
 * @param irq 中断号
 * @param handler 中断处理函数
 * @param arg 传递给处理函数的参数
 * @param name 中断名称
 * @return 0 成功，负数失败
 */
int pci_irq_install(uint32_t irq, pci_isr_handler_t handler, void *arg, const char *name)
{
    struct pci_dev *dev = NULL;
    ttos_irq_desc_t desc;
    unsigned long irqflags = 0;
    uint32_t actual_irq = irq;

    desc = ttos_pic_irq_desc_get(irq);
    if (desc)
    {
        dev = desc->pirv;
    }

    if (!dev)
    {
        return -1;
    }

    /* 根据 PCI 中断类型设置标志 */
    switch (dev->irq_type)
    {
    case PCI_INT_MSIX:
    case PCI_INT_MSI:
        irqflags = 0; /* MSI/MSI-X 不需要共享 */
        actual_irq = irq;
        break;

    case PCI_INT_LEGACY:
        irqflags = IRQ_SHARED; /* Legacy 中断可能共享 */
        actual_irq = dev->legacy_irq;
        break;

    default:
        return -1;
    }

    /* 使用托管中断注册，自动卸载 */
    return devm_request_irq(&dev->dev, actual_irq, (void (*)(unsigned int, void *))handler,
                            irqflags, name, arg);
}
