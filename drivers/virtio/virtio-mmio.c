/****************************************************************************
 * drivers/virtio/virtio-mmio.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/
 

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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "virtio-mmio.h"
#include "virtio.h"
#include <assert.h>
#include <driver/devbus.h>
#include <driver/device.h>
#include <driver/of.h>
#include <driver/platform.h>
#include <errno.h>
#include <inttypes.h>
#include <io.h>
#include <stdint.h>
#include <sys/param.h>
#include <system/types.h>
#include <ttosMM.h>
#include <ttos_init.h>
#include <ttos_pic.h>

#define KLOG_LEVEL KLOG_WARN
#define KLOG_TAG "virtio-mmio"
#include <klog.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define VIRITO_PAGE_SHIFT 12
#define VIRTIO_PAGE_SIZE (1 << VIRITO_PAGE_SHIFT)
#define VIRTIO_VRING_ALIGN VIRTIO_PAGE_SIZE

#define VIRTIO_MMIO_VERSION_1 1

/* Control registers */

/* Magic value ("virt" string) - Read Only */

#define VIRTIO_MMIO_MAGIC_VALUE 0x000
#define VIRTIO_MMIO_MAGIC_VALUE_STRING ('v' | ('i' << 8) | ('r' << 16) | ('t' << 24))

/* Virtio device version - Read Only */

#define VIRTIO_MMIO_VERSION 0x004

/* Virtio device ID - Read Only */

#define VIRTIO_MMIO_DEVICE_ID 0x008

/* Virtio vendor ID - Read Only */

#define VIRTIO_MMIO_VENDOR_ID 0x00c

/* Bitmask of the features supported by the device (host)
 * (32 bits per set) - Read Only
 */

#define VIRTIO_MMIO_DEVICE_FEATURES 0x010

/* Device (host) features set selector - Write Only */

#define VIRTIO_MMIO_DEVICE_FEATURES_SEL 0x014

/* Bitmask of features activated by the driver (guest)
 * (32 bits per set) - Write Only
 */

#define VIRTIO_MMIO_DRIVER_FEATURES 0x020

/* Activated features set selector - Write Only */

#define VIRTIO_MMIO_DRIVER_FEATURES_SEL 0x024

/* [VERSION 1 REGISTER] Guest page size */

#define VIRTIO_MMIO_PAGE_SIZE 0X028

/* Queue selector - Write Only */

#define VIRTIO_MMIO_QUEUE_SEL 0x030

/* Maximum size of the currently selected queue - Read Only */

#define VIRTIO_MMIO_QUEUE_NUM_MAX 0x034

/* Queue size for the currently selected queue - Write Only */

#define VIRTIO_MMIO_QUEUE_NUM 0x038

/* [VERSION 1 REGISTER] Used Ring alignment in the virtual queue */

#define VIRTIO_MMIO_QUEUE_ALIGN 0x03c

/* [VERSION 1 REGISTER] Guest physical page number of the virtual queue
 * Writing to this register notifies the device about location
 */

#define VIRTIO_MMIO_QUEUE_PFN 0x040

/* Ready bit for the currently selected queue - Read Write */

#define VIRTIO_MMIO_QUEUE_READY 0x044

/* Queue notifier - Write Only */

#define VIRTIO_MMIO_QUEUE_NOTIFY 0x050

/* Interrupt status - Read Only */

#define VIRTIO_MMIO_INTERRUPT_STATUS 0x060

/* Interrupt acknowledge - Write Only */

#define VIRTIO_MMIO_INTERRUPT_ACK 0x064
#define VIRTIO_MMIO_INTERRUPT_VRING (1 << 0)
#define VIRTIO_MMIO_INTERRUPT_CONFIG (1 << 1)

/* Device status register - Read Write */

#define VIRTIO_MMIO_STATUS 0x070

/* Selected queue's Descriptor Table address, 64 bits in two halves */

#define VIRTIO_MMIO_QUEUE_DESC_LOW 0x080
#define VIRTIO_MMIO_QUEUE_DESC_HIGH 0x084

/* Selected queue's Available Ring address, 64 bits in two halves */

#define VIRTIO_MMIO_QUEUE_AVAIL_LOW 0x090
#define VIRTIO_MMIO_QUEUE_AVAIL_HIGH 0x094

/* Selected queue's Used Ring address, 64 bits in two halves */

#define VIRTIO_MMIO_QUEUE_USED_LOW 0x0a0
#define VIRTIO_MMIO_QUEUE_USED_HIGH 0x0a4

/* Shared memory region id */

#define VIRTIO_MMIO_SHM_SEL 0x0ac

/* Shared memory region length, 64 bits in two halves */

#define VIRTIO_MMIO_SHM_LEN_LOW 0x0b0
#define VIRTIO_MMIO_SHM_LEN_HIGH 0x0b4

/* Shared memory region base address, 64 bits in two halves */

#define VIRTIO_MMIO_SHM_BASE_LOW 0x0b8
#define VIRTIO_MMIO_SHM_BASE_HIGH 0x0bc

/* Configuration atomicity value */

#define VIRTIO_MMIO_CONFIG_GENERATION 0x0fc

/* The config space is defined by each driver as
 * the per-driver configuration space - Read Write
 */

#define VIRTIO_MMIO_CONFIG 0x100

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct virtio_mmio_device_s
{
    struct virtio_device vdev; /* Virtio deivce */
    virt_addr_t cfg_addr;      /* Config memory virtual address */
    phys_addr_t cfg_phy;       /* Config memory physical address */
    int irq;                   /* The mmio interrupt number */
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/* Helper functions */

static uint32_t virtio_mmio_get_queue_len(virt_addr_t io, int idx);
static int virtio_mmio_config_virtqueue(virt_addr_t io, struct virtqueue *vq);
static int virtio_mmio_init_device(struct virtio_mmio_device_s *vmdev, phys_addr_t regs,
                                   size_t reg_size, int irq);

/* Virtio mmio dispatch functions */

static int virtio_mmio_create_virtqueue(struct virtio_mmio_device_s *vmdev, unsigned int i,
                                        const char *name, vq_callback callback);
static int virtio_mmio_create_virtqueues(struct virtio_device *vdev, unsigned int flags,
                                         unsigned int nvqs, const char *names[],
                                         vq_callback callbacks[]);
static void virtio_mmio_delete_virtqueues(struct virtio_device *vdev);
static void virtio_mmio_set_status(struct virtio_device *vdev, uint8_t status);
static uint8_t virtio_mmio_get_status(struct virtio_device *vdev);
static void virtio_mmio_write_config(struct virtio_device *vdev, uint32_t offset, void *dst,
                                     int length);
static void virtio_mmio_read_config(struct virtio_device *vdev, uint32_t offset, void *dst,
                                    int length);
static uint64_t virtio_mmio_get_features(struct virtio_device *vdev);
static void virtio_mmio_set_features(struct virtio_device *vdev, uint64_t features);
static uint64_t virtio_mmio_negotiate_features(struct virtio_device *vdev, uint64_t features);
static void virtio_mmio_reset_device(struct virtio_device *vdev);
static void virtio_mmio_notify(struct virtqueue *vq);

/* Interrupt */

static void virtio_mmio_interrupt(u32 irq, void *arg);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct virtio_dispatch g_virtio_mmio_dispatch = {
    virtio_mmio_create_virtqueues,  /* create_virtqueues */
    virtio_mmio_delete_virtqueues,  /* delete_virtqueues */
    virtio_mmio_get_status,         /* get_status */
    virtio_mmio_set_status,         /* set_status */
    virtio_mmio_get_features,       /* get_features */
    virtio_mmio_set_features,       /* set_features */
    virtio_mmio_negotiate_features, /* negotiate_features */
    virtio_mmio_read_config,        /* read_config */
    virtio_mmio_write_config,       /* write_config */
    virtio_mmio_reset_device,       /* reset_device */
    virtio_mmio_notify,             /* notify */
    NULL,                           /* notify_wait */
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static inline void virtio_mmio_write32(struct virtio_device *vdev, int offset, uint32_t value)
{
    struct virtio_mmio_device_s *vmdev = container_of(vdev, struct virtio_mmio_device_s, vdev);

    writel(value, vmdev->cfg_addr + offset);
}

static inline uint32_t virtio_mmio_read32(struct virtio_device *vdev, int offset)
{
    struct virtio_mmio_device_s *vmdev = container_of(vdev, struct virtio_mmio_device_s, vdev);

    return readl(vmdev->cfg_addr + offset);
}

static inline uint8_t virtio_mmio_read8(struct virtio_device *vdev, int offset)
{
    struct virtio_mmio_device_s *vmdev = container_of(vdev, struct virtio_mmio_device_s, vdev);

    return readb(vmdev->cfg_addr + offset);
}

/****************************************************************************
 * Name: virtio_mmio_get_queue_len
 ****************************************************************************/

static uint32_t virtio_mmio_get_queue_len(virt_addr_t io, int idx)
{
    uint32_t len;

    /* Select the queue we're interested in */
    writel(idx, io + VIRTIO_MMIO_QUEUE_SEL);
    len = readl(io + VIRTIO_MMIO_QUEUE_NUM_MAX);

    if (CONFIG_DRIVERS_VIRTIO_MMIO_QUEUE_LEN != 0)
    {
        len = MIN(len, CONFIG_DRIVERS_VIRTIO_MMIO_QUEUE_LEN);
    }

    return len;
}

/****************************************************************************
 * Name: virtio_mmio_config_virtqueue
 ****************************************************************************/

static int virtio_mmio_config_virtqueue(virt_addr_t io, struct virtqueue *vq)
{
    uint32_t version = vq->vq_dev->id.version;
    phys_addr_t addr;

    /* Select the queue we're interested in */

    writel(vq->vq_queue_index, io + VIRTIO_MMIO_QUEUE_SEL);

    /* Queue shouldn't already be set up. */

    if (readl(io +
              (version == VIRTIO_MMIO_VERSION_1 ? VIRTIO_MMIO_QUEUE_PFN : VIRTIO_MMIO_QUEUE_READY)))
    {
        KLOG_E("Virtio queue not ready");
        return -ENOENT;
    }

    /* Activate the queue */

    if (version == VIRTIO_MMIO_VERSION_1)
    {
        uint64_t pfn =
            (phys_addr_t)mm_kernel_v2p((virt_addr_t)vq->vq_ring.desc) >> VIRITO_PAGE_SHIFT;

        KLOG_I("Legacy, desc=%p, pfn=0x%" PRIx64 ", align=%d", vq->vq_ring.desc, pfn,
               VIRTIO_PAGE_SIZE);

        /* virtio-mmio v1 uses a 32bit QUEUE PFN. If we have something
         * that doesn't fit in 32bit, fail the setup rather than
         * pretending to be successful.
         */

        if (pfn >> 32)
        {
            KLOG_E("Legacy virtio-mmio used RAM shoud not above 0x%llxGB",
                   0x1ull << (2 + VIRITO_PAGE_SHIFT));
        }

        writel(vq->vq_nentries, io + VIRTIO_MMIO_QUEUE_NUM);
        writel(VIRTIO_PAGE_SIZE, io + VIRTIO_MMIO_QUEUE_ALIGN);
        writel(pfn, io + VIRTIO_MMIO_QUEUE_PFN);
    }
    else
    {
        writel(vq->vq_nentries, io + VIRTIO_MMIO_QUEUE_NUM);

        addr = (phys_addr_t)mm_kernel_v2p((virt_addr_t)vq->vq_ring.desc);
        writel(addr, io + VIRTIO_MMIO_QUEUE_DESC_LOW);
        writel(addr >> 32, io + VIRTIO_MMIO_QUEUE_DESC_HIGH);

        addr = (phys_addr_t)mm_kernel_v2p((virt_addr_t)vq->vq_ring.avail);
        writel(addr, io + VIRTIO_MMIO_QUEUE_AVAIL_LOW);
        writel(addr >> 32, io + VIRTIO_MMIO_QUEUE_AVAIL_HIGH);

        addr = (phys_addr_t)mm_kernel_v2p((virt_addr_t)vq->vq_ring.used);
        writel(addr, io + VIRTIO_MMIO_QUEUE_USED_LOW);
        writel(addr >> 32, io + VIRTIO_MMIO_QUEUE_USED_HIGH);

        writel(1, io + VIRTIO_MMIO_QUEUE_READY);
    }

    return 0;
}

/****************************************************************************
 * Name: virtio_mmio_create_virtqueue
 ****************************************************************************/

static int virtio_mmio_create_virtqueue(struct virtio_mmio_device_s *vmdev, unsigned int i,
                                        const char *name, vq_callback callback)
{
    struct virtio_device *vdev = &vmdev->vdev;
    struct virtio_vring_info *vrinfo;
    struct vring_alloc_info *vralloc;
    struct virtqueue *vq;
    int vringsize;
    int ret;

    /* Alloc virtqueue and init the vring info and vring alloc info */

    vrinfo = &vdev->vrings_info[i];
    vralloc = &vrinfo->info;
    vralloc->num_descs = virtio_mmio_get_queue_len(vmdev->cfg_addr, i);
    vq = virtqueue_allocate(vralloc->num_descs);
    if (vq == NULL)
    {
        KLOG_E("virtqueue_allocate failed");
        return -ENOMEM;
    }

    /* Init the vring info and vring alloc info */

    vrinfo->vq = vq;
    vrinfo->notifyid = i;
    vralloc->align = VIRTIO_VRING_ALIGN;
    vringsize = vring_size(vralloc->num_descs, VIRTIO_VRING_ALIGN);
    vralloc->vaddr = virtio_zalloc_buf(vdev, vringsize, VIRTIO_VRING_ALIGN);
    if (vralloc->vaddr == NULL)
    {
        KLOG_E("vring alloc failed");
        return -ENOMEM;
    }

    /* Initialize the virtio queue */

    ret = virtqueue_create(vdev, i, name, vralloc, callback, vdev->func->notify, vq);
    if (ret < 0)
    {
        KLOG_E("virtqueue create error, ret=%d", ret);
        return ret;
    }

    /* Set the mmio virtqueue register */

    ret = virtio_mmio_config_virtqueue(vmdev->cfg_addr, vq);
    if (ret < 0)
    {
        KLOG_E("virtio_mmio_config_virtqueue failed, ret=%d", ret);
    }

    return ret;
}

/****************************************************************************
 * Name: virtio_mmio_create_virtqueues
 ****************************************************************************/

static int virtio_mmio_create_virtqueues(struct virtio_device *vdev, unsigned int flags,
                                         unsigned int nvqs, const char *names[],
                                         vq_callback callbacks[])
{
    struct virtio_mmio_device_s *vmdev = (struct virtio_mmio_device_s *)vdev;
    unsigned int i;
    int ret = 0;

    /* 使用托管内存分配 vring info */
    vdev->vrings_num = nvqs;
    vdev->vrings_info =
        devm_kzalloc(vdev->device.parent, sizeof(struct virtio_vring_info) * nvqs, GFP_KERNEL);
    if (vdev->vrings_info == NULL)
    {
        KLOG_E("alloc vrings info failed");
        return -ENOMEM;
    }

    /* Alloc and init the virtqueue */

    for (i = 0; i < nvqs; i++)
    {
        ret = virtio_mmio_create_virtqueue(vmdev, i, names[i], callbacks[i]);
        if (ret < 0)
        {
            goto err;
        }
    }

    /* Finally, enable the interrupt */
    ttos_pic_irq_unmask(vmdev->irq);
    return ret;

err:
    virtio_mmio_delete_virtqueues(vdev);
    return ret;
}

/****************************************************************************
 * Name: virtio_mmio_delete_virtqueues
 ****************************************************************************/

static void virtio_mmio_delete_virtqueues(struct virtio_device *vdev)
{
    struct virtio_mmio_device_s *vmdev = (struct virtio_mmio_device_s *)vdev;
    struct virtio_vring_info *vrinfo;
    unsigned int i;

    /* Disable interrupt first */

    ttos_pic_irq_mask(vmdev->irq);

    /* Free the memory */

    if (vdev->vrings_info != NULL)
    {
        for (i = 0; i < vdev->vrings_num; i++)
        {
            writel(i, vmdev->cfg_addr + VIRTIO_MMIO_QUEUE_SEL);
            if (vdev->id.version == VIRTIO_MMIO_VERSION_1)
            {
                writel(0, vmdev->cfg_addr + VIRTIO_MMIO_QUEUE_PFN);
            }
            else
            {
                /* Virtio 1.2: To stop using the queue the driver MUST write
                 * zero (0x0) to this QueueReady and MUST read the value back
                 * to ensure synchronization.
                 */

                writel(0, vmdev->cfg_addr + VIRTIO_MMIO_QUEUE_READY);
                if (readl(vmdev->cfg_addr + VIRTIO_MMIO_QUEUE_READY))
                {
                    KLOG_W("queue ready set zero failed");
                }
            }

            /* Free the vring buffer and virtqueue */

            vrinfo = &vdev->vrings_info[i];
            if (vrinfo->info.vaddr != NULL)
            {
                virtio_free_buf(vdev, vrinfo->info.vaddr);
            }

            if (vrinfo->vq != NULL)
            {
                virtqueue_free(vrinfo->vq);
            }
        }

        /* vrings_info 由 devres 自动释放 */
    }
}

/****************************************************************************
 * Name: virtio_mmio_set_status
 ****************************************************************************/

static void virtio_mmio_set_status(struct virtio_device *vdev, uint8_t status)
{
    virtio_mmio_write32(vdev, VIRTIO_MMIO_STATUS, status);
}

/****************************************************************************
 * Name: virtio_mmio_get_status
 ****************************************************************************/

static uint8_t virtio_mmio_get_status(struct virtio_device *vdev)
{
    return virtio_mmio_read32(vdev, VIRTIO_MMIO_STATUS);
}

/****************************************************************************
 * Name: virtio_mmio_write_config
 ****************************************************************************/

static void virtio_mmio_write_config(struct virtio_device *vdev, uint32_t offset, void *src,
                                     int length)
{
    struct virtio_mmio_device_s *vmdev = (struct virtio_mmio_device_s *)vdev;
    uint32_t write_offset = VIRTIO_MMIO_CONFIG + offset;
    uint32_t u32data;
    uint16_t u16data;
    uint8_t u8data;

    if (vdev->id.version == VIRTIO_MMIO_VERSION_1)
    {
        goto byte_write;
    }

    switch (length)
    {
    case 1:
        memcpy(&u8data, src, sizeof(u8data));
        writeb(u8data, vmdev->cfg_addr + write_offset);
        break;
    case 2:
        memcpy(&u16data, src, sizeof(u16data));
        writew(u16data, vmdev->cfg_addr + write_offset);
        break;
    case 4:
        memcpy(&u32data, src, sizeof(u32data));
        writel(u32data, vmdev->cfg_addr + write_offset);
        break;
    case 8:
        memcpy(&u32data, src, sizeof(u32data));
        writel(u32data, vmdev->cfg_addr + write_offset);
        memcpy(&u32data, src + sizeof(u32data), sizeof(u32data));
        writel(u32data, vmdev->cfg_addr + write_offset + sizeof(u32data));
        break;
    default:
    byte_write:
    {
        char *s = src;
        int i;
        for (i = 0; i < length; i++)
        {
            writeb(s[i], vmdev->cfg_addr + write_offset + i);
        }
    }
    }
}

/****************************************************************************
 * Name: virtio_mmio_read_config
 ****************************************************************************/

static void virtio_mmio_read_config(struct virtio_device *vdev, uint32_t offset, void *dst,
                                    int length)
{
    struct virtio_mmio_device_s *vmdev = (struct virtio_mmio_device_s *)vdev;
    uint32_t read_offset = VIRTIO_MMIO_CONFIG + offset;
    uint32_t u32data;
    uint16_t u16data;
    uint8_t u8data;

    if (vdev->id.version == VIRTIO_MMIO_VERSION_1)
    {
        goto byte_read;
    }

    switch (length)
    {
    case 1:
        u8data = readb(vmdev->cfg_addr + read_offset);
        memcpy(dst, &u8data, sizeof(u8data));
        break;
    case 2:
        u16data = readw(vmdev->cfg_addr + read_offset);
        memcpy(dst, &u16data, sizeof(u16data));
        break;
    case 4:
        u32data = readl(vmdev->cfg_addr + read_offset);
        memcpy(dst, &u32data, sizeof(u32data));
        break;
    case 8:
        u32data = readl(vmdev->cfg_addr + read_offset);
        memcpy(dst, &u32data, sizeof(u32data));
        u32data = readl(vmdev->cfg_addr + read_offset + sizeof(u32data));
        memcpy(dst + sizeof(u32data), &u32data, sizeof(u32data));
        break;
    default:
    byte_read:
    {
        char *d = dst;
        int i;
        for (i = 0; i < length; i++)
        {
            d[i] = readb(vmdev->cfg_addr + read_offset + i);
        }
    }
    }
}

/****************************************************************************
 * Name: virtio_mmio_get_features
 ****************************************************************************/

static uint64_t virtio_mmio_get_features(struct virtio_device *vdev)
{
    struct virtio_mmio_device_s *vmdev = (struct virtio_mmio_device_s *)vdev;

    writel(0, vmdev->cfg_addr + VIRTIO_MMIO_DRIVER_FEATURES_SEL);
    return readq(vmdev->cfg_addr + VIRTIO_MMIO_DEVICE_FEATURES);
}

/****************************************************************************
 * Name: virtio_mmio_set_features
 ****************************************************************************/

static void virtio_mmio_set_features(struct virtio_device *vdev, uint64_t features)
{
    struct virtio_mmio_device_s *vmdev = (struct virtio_mmio_device_s *)vdev;

    writel(0, vmdev->cfg_addr + VIRTIO_MMIO_DRIVER_FEATURES_SEL);
    writel(features, vmdev->cfg_addr + VIRTIO_MMIO_DRIVER_FEATURES);
    vdev->features = features;
}

/****************************************************************************
 * Name: virtio_mmio_negotiate_features
 ****************************************************************************/

static uint64_t virtio_mmio_negotiate_features(struct virtio_device *vdev, uint64_t features)
{
    features = features & virtio_mmio_get_features(vdev);
    virtio_mmio_set_features(vdev, features);
    return features;
}

/****************************************************************************
 * Name: virtio_mmio_reset_device
 ****************************************************************************/

static void virtio_mmio_reset_device(struct virtio_device *vdev)
{
    virtio_mmio_set_status(vdev, VIRTIO_CONFIG_STATUS_RESET);
}

/****************************************************************************
 * Name: virtio_mmio_notify
 ****************************************************************************/

static void virtio_mmio_notify(struct virtqueue *vq)
{
    struct virtio_mmio_device_s *vmdev = (struct virtio_mmio_device_s *)vq->vq_dev;

    /* VIRTIO_F_NOTIFICATION_DATA is not supported for now */

    writel(vq->vq_queue_index, vmdev->cfg_addr + VIRTIO_MMIO_QUEUE_NOTIFY);
}

/****************************************************************************
 * Name: virtio_mmio_interrupt
 ****************************************************************************/
static void virtio_mmio_interrupt(u32 irq, void *arg)
{
    struct virtio_device *vdev = arg;
    struct virtio_vring_info *vrings_info = vdev->vrings_info;
    struct virtqueue *vq;
    unsigned int i;
    uint32_t isr = virtio_mmio_read32(vdev, VIRTIO_MMIO_INTERRUPT_STATUS);
    if (isr & VIRTIO_MMIO_INTERRUPT_VRING)
    {
        for (i = 0; i < vdev->vrings_num; i++)
        {
            vq = vrings_info[i].vq;
            if (vq->callback)
                vq->callback(vq);
        }
    }
    virtio_mmio_write32(vdev, VIRTIO_MMIO_INTERRUPT_ACK, isr);
}

/****************************************************************************
 * Name: virtio_mmio_init_device
 ****************************************************************************/

static int virtio_mmio_init_device(struct virtio_mmio_device_s *vmdev, phys_addr_t regs,
                                   size_t reg_size, int irq)
{
    struct virtio_device *vdev = &vmdev->vdev;
    uint32_t magic;

    /* Save the irq */

    vmdev->irq = irq;

    /* Share memory io is used for the virtio buffer operations
     * Config memory is used for the mmio register operations
     */
    vmdev->cfg_phy = (phys_addr_t)regs;
    vmdev->cfg_addr = (virt_addr_t)ioremap(vmdev->cfg_phy, reg_size);

    /* Init the virtio device */

    vdev->role = VIRTIO_DEV_DRIVER;
    vdev->func = &g_virtio_mmio_dispatch;

    magic = readl(vmdev->cfg_addr + VIRTIO_MMIO_MAGIC_VALUE);
    if (magic != VIRTIO_MMIO_MAGIC_VALUE_STRING)
    {
        KLOG_E("Bad magic value %" PRIu32 "", magic);
        iounmap(vmdev->cfg_addr);
        return -EINVAL;
    }

    vdev->id.version = readl(vmdev->cfg_addr + VIRTIO_MMIO_VERSION);
    vdev->id.device = readl(vmdev->cfg_addr + VIRTIO_MMIO_DEVICE_ID);
    if (vdev->id.device == 0)
    {
        iounmap(vmdev->cfg_addr);
        return -ENODEV;
    }

    vdev->id.vendor = readl(vmdev->cfg_addr + VIRTIO_MMIO_VENDOR_ID);

    /* Legacy mmio version, set the page size */

    if (vdev->id.version == VIRTIO_MMIO_VERSION_1)
    {
        writel(VIRTIO_PAGE_SIZE, vmdev->cfg_addr + VIRTIO_MMIO_PAGE_SIZE);
    }

    KLOG_I("VIRTIO version: %" PRIu32 " device: %" PRIu32 " vendor: %" PRIx32 "", vdev->id.version,
           vdev->id.device, vdev->id.vendor);

    /* Reset the virtio device and set ACK */

    virtio_mmio_set_status(vdev, VIRTIO_CONFIG_STATUS_RESET);
    virtio_mmio_set_status(vdev, VIRTIO_CONFIG_STATUS_ACK);
    return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: virtio_register_mmio_device
 *
 * Description:
 *   Register virtio mmio device to the virtio bus
 *
 ****************************************************************************/

static void virtio_mmio_device_release(struct device *dev)
{
    of_node_put(dev->of_node);
    dev->of_node = NULL;
    free(dev);
}

static int virtio_mmio_probe(struct platform_device *pdev)
{
    int irq;
    int ret = 0;
    struct virtio_mmio_device_s *vmdev;
    struct device *dev = &pdev->dev;
    phys_addr_t reg_addr;
    size_t reg_size;

    /* 使用托管内存分配 */
    vmdev = devm_kzalloc(dev, sizeof(struct virtio_mmio_device_s), GFP_KERNEL);
    if (!vmdev)
        return -ENOMEM;

    initialize_device(&vmdev->vdev.device);

    vmdev->vdev.device.of_node = of_node_get(dev->of_node);
    vmdev->vdev.device.parent = dev;
    vmdev->vdev.device.release = virtio_mmio_device_release;
    vmdev->vdev.device.priv = NULL;

    ret = of_device_get_resource_reg(dev, &reg_addr, &reg_size);
    if (ret < 0)
    {
        KLOG_E("device get reg error");
        return ret;
    }
    KLOG_D("reg: %llx, size: 0x%x", reg_addr, reg_size);

    irq = ttos_pic_irq_alloc(dev, 0);
    KLOG_D("virq: %d", irq);

    if (!irq)
    {
        KLOG_E("irq alloc failed");
        return -EINVAL;
    }

    ret = virtio_mmio_init_device(vmdev, reg_addr, reg_size, irq);
    if (ret < 0)
    {
        KLOG_D("No virtio mmio device in regs=0x%" PRIxPTR, vmdev->cfg_addr);
        return ret;
    }

    /* 使用托管中断注册 */
    ret = devm_request_irq(dev, irq, virtio_mmio_interrupt, 0, "vmmio", &vmdev->vdev);
    if (ret < 0)
    {
        KLOG_E("irq_attach failed, ret=%d", ret);
        return ret;
    }

    ret = virtio_device_register(&vmdev->vdev);
    if (ret)
        return ret;

    return 0;
}

static struct of_device_id virtio_mmio_table[] = {
    {.compatible = "virtio,mmio"},
    {/* end of list */},
};

static struct platform_driver virtio_mmio_device_driver = {
    .probe = virtio_mmio_probe,
    .driver = {.name = "virtio-mmio", .of_match_table = virtio_mmio_table},
};
int virtio_mmio_driver_init(void)
{
    return platform_add_driver(&virtio_mmio_device_driver);
}
#ifndef __x86_64__
INIT_EXPORT_DRIVER(virtio_mmio_driver_init, "virtio_driver_init");
#endif
