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

#include <assert.h>
#include <driver/of.h>
#include <errno.h>
#include <fs/fs.h>
#include <fs/ioctl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <ttos.h>
#include <ttosMM.h>
#include <ttos_init.h>
#include <util.h>

#undef KLOG_TAG
#undef KLOG_LEVEL
#define KLOG_TAG "RAMDISK"
#define KLOG_LEVEL KLOG_DEBUG
#include <klog.h>

struct ramdisk_device
{
    char *name;
    void *start_address;
    void *end_address;
    size_t size;
    uint32_t sector_size;
};

/****************************************************************************
 * Name: virtio_blk_open
 *
 * Description: Open the block device
 *
 ****************************************************************************/

static int ramdisk_blk_open(struct inode *inode)
{
    assert(inode->i_private);
    return 0;
}

/****************************************************************************
 * Name: virtio_blk_close
 *
 * Description: close the block device
 *
 ****************************************************************************/

static int ramdisk_blk_close(struct inode *inode)
{
    assert(inode->i_private);
    return 0;
}

/****************************************************************************
 * Name: virtio_blk_read
 *
 * Description:
 *   Read the specified number of sectors from the read-ahead buffer or from
 *   the physical device.
 *
 ****************************************************************************/

static ssize_t ramdisk_blk_read(struct inode *inode, unsigned char *buffer, blkcnt_t startsector,
                                unsigned int nsectors)
{
    struct ramdisk_device *dev;
    void *start_addr;
    void *end_addr;

    assert(inode->i_private);
    dev = inode->i_private;

    start_addr = (void *)(long)(startsector * dev->sector_size) + (long)dev->start_address;
    end_addr = start_addr + nsectors * dev->sector_size;

    if (end_addr > dev->end_address)
    {
        KLOG_E("ramdisk read parm err");
        return 0;
    }

    memcpy(buffer, (void *)start_addr, nsectors * dev->sector_size);

    return nsectors;
}

/****************************************************************************
 * Name: virtio_blk_write
 *
 * Description:
 *   Write the specified number of sectors to the write buffer or to the
 *   physical device.
 *
 ****************************************************************************/

static ssize_t ramdisk_blk_write(struct inode *inode, const unsigned char *buffer,
                                 blkcnt_t startsector, unsigned int nsectors)
{
    struct ramdisk_device *dev;
    void *start_addr;
    void *end_addr;

    assert(inode->i_private);
    dev = inode->i_private;

    start_addr = (void *)(long)(startsector * dev->sector_size) + (long)dev->start_address;
    end_addr = start_addr + nsectors * dev->sector_size;

    if (end_addr > dev->end_address)
    {
        KLOG_E("ramdisk write parm err");
        return 0;
    }

    memcpy((void *)start_addr, buffer, nsectors * dev->sector_size);

    return nsectors;
}

/****************************************************************************
 * Name: virtio_blk_geometry
 *
 * Description: Return device geometry
 *
 ****************************************************************************/

static int ramdisk_blk_geometry(struct inode *inode, struct geometry *geometry)
{
    struct ramdisk_device *dev;
    int ret = -EINVAL;

    assert(inode->i_private);
    dev = inode->i_private;

    if (geometry)
    {
        geometry->geo_available = true;
        geometry->geo_mediachanged = false;
        geometry->geo_writeenabled = true;
        geometry->geo_nsectors = (dev->end_address - dev->start_address) / dev->sector_size;
        geometry->geo_sectorsize = dev->sector_size;
        ret = 0;
    }

    return ret;
}

static int ramdisk_blk_ioctl(struct inode *inode, int cmd, unsigned long arg)
{
    int ret = -ENOTTY;

    return ret;
}

static const struct block_operations g_ramdisk_blk_bops = {
    ramdisk_blk_open,     /* open     */
    ramdisk_blk_close,    /* close    */
    ramdisk_blk_read,     /* read     */
    ramdisk_blk_write,    /* write    */
    ramdisk_blk_geometry, /* geometry */
    ramdisk_blk_ioctl     /* ioctl    */
};

struct ramdisk_device *ramdisk_create(const char *name, void *start_addr, size_t size,
                                      uint32_t sector_size)
{
    struct ramdisk_device *config;
    static uint32_t index;
    static uint32_t len;
    char name_str[12];

    if ((size == 0) || (sector_size == 0))
    {
        return NULL;
    }

    config = malloc(sizeof(struct ramdisk_device));
    if (!config)
    {
        return NULL;
    }

    if (!name || !(len = strlen(name)))
    {
        len = snprintf(name_str, 12, "ramdisk%d", index);
        index++;
    }

    len++;
    config->name = malloc(len);
    strncpy(config->name, name, len);

    config->start_address = start_addr;
    config->end_address = start_addr + size - 1;
    config->size = size;
    config->sector_size = sector_size;

    return config;
}

int ramdisk_driver_register(struct ramdisk_device *device)
{
    int ret;
    char path[64];

    snprintf(path, 64, "/dev/%s", device->name);

    ret = register_blockdriver(path, &g_ramdisk_blk_bops, 0660, device);
    if (ret < 0)
    {
        KLOG_E("Register block driver failed, ret=%d", ret);
    }

    return ret;
}

static int32_t initrd_probe(void)
{
    const void *fdt;
    const void *initrd_start;
    const void *initrd_end;
    int len = 0;
    int chosen_offset;
    uint64_t initrd[2] = {0, 0};

    fdt = fdt_root_get();

    chosen_offset = fdt_path_offset(fdt, "/chosen");
    if (!chosen_offset)
        return 0;

    initrd_start = fdt_getprop(fdt, chosen_offset, "linux,initrd-start", &len);
    if (initrd_start)
    {
        if (len == sizeof(uint32_t))
        {
            initrd[0] = fdt32_to_cpu(*(uint32_t *)initrd_start);
        }
        else if (len == sizeof(uint64_t))
        {
            initrd[0] = fdt64_to_cpu(*(uint64_t *)initrd_start);
        }
    }

    initrd_end = fdt_getprop(fdt, chosen_offset, "linux,initrd-end", &len);
    if (initrd_end)
    {
        if (len == sizeof(uint32_t))
        {
            initrd[1] = fdt32_to_cpu(*(uint32_t *)initrd_end);
        }
        else if (len == sizeof(uint64_t))
        {
            initrd[1] = fdt64_to_cpu(*(uint64_t *)initrd_end);
        }
    }

    if (initrd[0] && initrd[1])
    {
        struct mm_region *region = calloc(1, sizeof(struct mm_region));

        mm_region_init(region, MT_RW_DATA | MT_KERNEL, initrd[0], initrd[1] - initrd[0]);
        region->is_need_free = true;

        mm_region_map(get_kernel_mm(), region);

        struct ramdisk_device *device;
        device = ramdisk_create("_initrd", (void *)region->virtual_address,
                                region->region_page_count * ttosGetPageSize(), 512);

        if (device)
        {
            ramdisk_driver_register(device);
        }
    }

    return 0;
}

#ifdef __x86_64__

extern bool g_multiboot_initrd;
extern phys_addr_t g_multiboot_initrd_start;
extern phys_addr_t g_multiboot_initrd_end;
static int32_t initrd_init(void)
{
    if (!g_multiboot_initrd)
    {
        return 0;
    }
    phys_addr_t initrd_start = g_multiboot_initrd_start;
    phys_addr_t initrd_end = g_multiboot_initrd_end;
    struct ramdisk_device *device = NULL;
    int32_t len = (int32_t)(initrd_end - initrd_start);

    len = round_up(len, PAGE_SIZE);

    struct mm_region *region = calloc(1, sizeof(struct mm_region));

    mm_region_init(region, MT_RW_DATA | MT_KERNEL, initrd_start, len);
    region->is_need_free = true;

    mm_region_map(get_kernel_mm(), region);

    printk("ramdisk pa:%" PRIx64 " va:%" PRIxPTR " size:0x%" PRIx64 "\n", initrd_start,
           region->virtual_address, region->region_page_count * ttosGetPageSize());

    device = ramdisk_create("_initrd", (void *)region->virtual_address,
                            region->region_page_count * ttosGetPageSize(), 4096);

    if (device)
    {
        ramdisk_driver_register(device);
    }

    return 0;
}

// INIT_EXPORT_PRE_DRIVER(initrd_init, "init ramdisk");
#else
INIT_EXPORT_PRE_DRIVER(initrd_probe, "init ramdisk");
#endif
