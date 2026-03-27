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
#include <stdio.h>
#include <ttosMM.h>
#include <ttos_pic.h>

static uint64_t read_dt_number(uint32_t *data, uint32_t size)
{
    uint64_t r = 0;
    for (; size--; data++)
        r = (r << 32) | *data;
    return r;
}

int pci_host_dt_parmes(struct pci_host *host)
{
    int ret = -1;
    uint32_t *ranges;
    uint32_t rang_num, i;
    uint32_t size_cells, addr_cells, pci_cells;
    uint32_t msi_phandle;
    struct pci_iomem mem32, mem64, pre_mem32, pre_mem64;
    struct device_node *msi_pic;

    const struct device_node *np;

    np = (const struct device_node *)host->dev.parent->of_node;
    if (!np)
    {
        goto exit;
    }

    const void *fdt;
    const void *pci_param = NULL;
    int len = 0;
    int chosen_offset;
    uint32_t val;
    uint32_t *param;

    fdt = fdt_root_get();

    chosen_offset = fdt_path_offset(fdt, "/chosen");
    if (chosen_offset)
    {
        pci_param = fdt_getprop(fdt, chosen_offset, "linux,pci-probe-only", &len);
    }

    if (pci_param)
    {
        val = fdt32_to_cpu(*(uint32_t *)pci_param);
        host->flag |= val ? PCI_HOST_FLAG_PROBE_ONLY : 0;
    }
    of_get_property(np, "interrupt-map", &len);

    len /= sizeof(*param);
    param = calloc(len, sizeof(*param));
    of_property_read_u32_array(np, "interrupt-map", param, len);

    for (int i = 0; i < 4; i++)
    {
        host->legacy_irq[i] = ttos_pic_irq_alloc(NULL, 32 + param[i * 10 + 8]);
    }

    free(param);

    if (of_property_match_string(np, "device_type", "pci"))
    {
        goto exit;
    }

    of_property_read_u32(np, "#size-cells", &size_cells);
    of_property_read_u32(np, "#address-cells", &addr_cells);
    rang_num = of_property_count_u32_elems(np, "ranges");

    pci_cells = size_cells * addr_cells + 1;

    ranges = calloc(1, rang_num * sizeof(uint32_t));
    if (of_property_read_u32_array(np, "ranges", ranges, rang_num) < 0)
    {
        goto exit1;
    }

    int msi_map_size = of_property_count_u32_elems(np, "msi-map");

    if (msi_map_size > 0)
    {
        struct
        {
            uint32_t input_base;
            uint32_t phandle;
            uint32_t output_base;
            uint32_t count;
        } * msi_map_dt;

        msi_map_dt = calloc(msi_map_size, sizeof(uint32_t));
        host->msi_map = calloc(msi_map_size / 4, sizeof(struct msi_map));
        of_property_read_u32_array(np, "msi-map", (uint32_t *)msi_map_dt, msi_map_size);
        host->msi_map_count = msi_map_size / 4;
        for (i = 0; i < host->msi_map_count; i++)
        {
            host->msi_map[i].input_base = msi_map_dt[i].input_base;
            host->msi_map[i].output_base = msi_map_dt[i].output_base;
            host->msi_map[i].count = msi_map_dt[i].count;

            msi_pic = of_find_node_by_phandle(msi_map_dt[i].phandle);
            if (msi_pic)
            {
                host->msi_map[i].pic = dev_priv_get((struct device *)msi_pic->device);
            }
        }
        free(msi_map_dt);
    }
    else if (!of_property_read_u32(np, "msi-parent", &msi_phandle))
    {
        msi_pic = of_find_node_by_phandle(msi_phandle);
        if (msi_pic)
        {
            host->pic = dev_priv_get((struct device *)msi_pic->device);
        }
    }

    memset(&mem32, 0, sizeof(struct pci_iomem));
    memset(&mem64, 0, sizeof(struct pci_iomem));
    memset(&pre_mem32, 0, sizeof(struct pci_iomem));
    memset(&pre_mem64, 0, sizeof(struct pci_iomem));
    rang_num = (rang_num / pci_cells) * pci_cells;

    for (i = 0; i < rang_num; i += pci_cells)
    {
        uint32_t flags = ranges[i];
        bool prefetch = flags & (1U << 30);

        switch ((flags >> 24) & 0x3)
        {
        case 0:
            host->cfg.paddr = read_dt_number(&ranges[i + (1 * size_cells) + 1], size_cells);
            host->cfg.size = read_dt_number(&ranges[i + (2 * size_cells) + 1], size_cells);
            break;

        case 1:
            host->io.pci.addr = read_dt_number(&ranges[i + 0 * size_cells + 1], size_cells);
            host->io.cpu.addr = read_dt_number(&ranges[i + 1 * size_cells + 1], size_cells);
            host->io.cpu.size = read_dt_number(&ranges[i + 2 * size_cells + 1], size_cells);
            host->io.pci.size = host->io.cpu.size;
            break;

        case 2:
            if (prefetch)
            {
                if (!pre_mem32.pci.size)
                {
                    pre_mem32.pci.addr =
                        read_dt_number(&ranges[i + 0 * size_cells + 1], size_cells);
                    pre_mem32.cpu.addr =
                        read_dt_number(&ranges[i + 1 * size_cells + 1], size_cells);
                    pre_mem32.cpu.size =
                        read_dt_number(&ranges[i + 2 * size_cells + 1], size_cells);
                    pre_mem32.pci.size = pre_mem32.cpu.size;
                }
            }
            else
            {
                if (!mem32.pci.size)
                {
                    mem32.pci.addr = read_dt_number(&ranges[i + 0 * size_cells + 1], size_cells);
                    mem32.cpu.addr = read_dt_number(&ranges[i + 1 * size_cells + 1], size_cells);
                    mem32.cpu.size = read_dt_number(&ranges[i + 2 * size_cells + 1], size_cells);
                    mem32.pci.size = mem32.cpu.size;
                }
            }
            break;

        case 3:
            if (prefetch)
            {
                if (!pre_mem64.pci.size)
                {
                    pre_mem64.pci.addr =
                        read_dt_number(&ranges[i + 0 * size_cells + 1], size_cells);
                    pre_mem64.cpu.addr =
                        read_dt_number(&ranges[i + 1 * size_cells + 1], size_cells);
                    pre_mem64.cpu.size =
                        read_dt_number(&ranges[i + 2 * size_cells + 1], size_cells);
                    pre_mem64.pci.size = pre_mem64.cpu.size;
                }
            }
            else
            {
                if (!mem64.pci.size)
                {
                    mem64.pci.addr = read_dt_number(&ranges[i + 0 * size_cells + 1], size_cells);
                    mem64.cpu.addr = read_dt_number(&ranges[i + 1 * size_cells + 1], size_cells);
                    mem64.cpu.size = read_dt_number(&ranges[i + 2 * size_cells + 1], size_cells);
                    mem64.pci.size = mem64.cpu.size;
                }
            }
            break;
        }
    }
    if (mem32.pci.size)
    {
        host->mem = mem32;
    }
    else if (mem64.pci.size)
    {
        host->mem = mem64;
    }

    if (pre_mem64.pci.size)
    {
        host->pre_mem = pre_mem64;
    }
    else if (pre_mem32.pci.size)
    {
        host->pre_mem = pre_mem32;
    }

    if (!host->cfg.size)
    {
        rang_num = of_property_count_u32_elems(np, "reg");
        of_property_read_u32_array(np, "reg", ranges, rang_num);

        host->cfg.paddr = read_dt_number(ranges, size_cells);
        host->cfg.size = read_dt_number(ranges + size_cells, size_cells);
    }

    if (host->cfg.size > 128 * 1024 * 1024)
    {
        host->cfg.size = 128 * 1024 * 1024;
    }

#ifdef PCI_DEBUG
    printk("host->cfg.paddr          0x%llX\n", host->cfg.paddr);
    printk("host->cfg.size           0x%X\n", host->cfg.size);
    printk("host->io.cpu.addr        0x%llX\n", host->io.cpu.addr);
    printk("host->io.pci.addr        0x%llX\n", host->io.pci.addr);
    printk("host->io.pci.size        0x%llX\n", host->io.pci.size);
    printk("host->mem.cpu.addr       0x%llX\n", host->mem.cpu.addr);
    printk("host->mem.pci.addr       0x%llX\n", host->mem.pci.addr);
    printk("host->mem.pci.size       0x%llX\n", host->mem.pci.size);
    printk("host->pre_mem.cpu.addr   0x%llX\n", host->pre_mem.cpu.addr);
    printk("host->pre_mem.pci.addr   0x%llX\n", host->pre_mem.pci.addr);
    printk("host->pre_mem.pci.size   0x%llX\n", host->pre_mem.pci.size);
#endif

    if (host->cfg.paddr && host->cfg.size)
    {
        host->cfg.vaddr = (virt_addr_t)ioremap(host->cfg.paddr, host->cfg.size);
    }

    host->bus_ranges.start = 0;
    host->bus_ranges.end = 0xFF;
    ret = 0;

exit1:
    free(ranges);
exit:
    return ret;
}
