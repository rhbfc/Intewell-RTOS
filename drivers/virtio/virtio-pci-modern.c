/****************************************************************************
 * drivers/virtio/virtio-pci-modern.c
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
 
/****************************************************************************
 * 本文件基于 Apache NuttX 对应 virtio 实现移植到当前 RTOS，
 * 并已针对本地接口和平台环境进行了修改。
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include <driver/pci/pci.h>
#include <driver/pci/pci_regs.h>
#include <errno.h>
#include <stdint.h>
#include <sys/param.h>
#include <ttosMM.h>
#include <uaccess.h>

#include "virtio-pci.h"
#include "virtio_helper.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Common configuration */

#define VIRTIO_PCI_CAP_COMMON_CFG 1

/* Notifications */

#define VIRTIO_PCI_CAP_NOTIFY_CFG 2

/* ISR Status */

#define VIRTIO_PCI_CAP_ISR_CFG 3

/* Device specific configuration */

#define VIRTIO_PCI_CAP_DEVICE_CFG 4

/* PCI configuration access */

#define VIRTIO_PCI_CAP_PCI_CFG 5

/* Shared memory region */

#define VIRTIO_PCI_CAP_SHARED_MEMORY_CFG 8

/* Vendor-specific data */

#define VIRTIO_PCI_CAP_VENDOR_CFG 9

/****************************************************************************
 * Private Types
 ****************************************************************************/

begin_packed_struct struct virtio_pci_cap_s
{
    uint8_t cap_vndr;   /* Generic PCI field: PCI_CAP_ID_VNDR */
    uint8_t cap_next;   /* Generic PCI field: next ptr. */
    uint8_t cap_len;    /* Generic PCI field: capability length */
    uint8_t cfg_type;   /* Identifies the structure. */
    uint8_t bar;        /* Where to find it. */
    uint8_t id;         /* Multiple capabilities of the same type */
    uint8_t padding[2]; /* Pad to full dword. */
    uint32_t offset;    /* Offset within bar. */
    uint32_t length;    /* Length of the structure, in bytes. */
} end_packed_struct;

begin_packed_struct struct virtio_pci_cap64_s
{
    struct virtio_pci_cap_s cap;
    uint32_t offset_hi;
    uint32_t length_hi;
} end_packed_struct;

/* VIRTIO_PCI_CAP_COMMON_CFG */

begin_packed_struct struct virtio_pci_common_cfg_s
{
    /* About the whole device. */

    uint32_t device_feature_select; /* read-write */
    uint32_t device_feature;        /* read-only for driver */
    uint32_t driver_feature_select; /* read-write */
    uint32_t driver_feature;        /* read-write */
    uint16_t config_msix_vector;    /* read-write */
    uint16_t num_queues;            /* read-only for driver */
    uint8_t device_status;          /* read-write */
    uint8_t config_generation;      /* read-only for driver */

    /* About a specific virtqueue. */

    uint16_t queue_select;      /* read-write */
    uint16_t queue_size;        /* read-write */
    uint16_t queue_msix_vector; /* read-write */
    uint16_t queue_enable;      /* read-write */
    uint16_t queue_notify_off;  /* read-only for driver */
    uint64_t queue_desc;        /* read-write */
    uint64_t queue_avail;       /* read-write */
    uint64_t queue_used;        /* read-write */
    uint16_t queue_notify_data; /* read-only for driver */
    uint16_t queue_reset;       /* read-write */
} end_packed_struct;

/* VIRTIO_PCI_CAP_NOTIFY_CFG */

begin_packed_struct struct virtio_pci_notify_cap_s
{
    struct virtio_pci_cap_s cap;

    /* Multiplier for queue_notify_off. */

    uint32_t notify_off_multiplier;
} end_packed_struct;

/* VIRTIO_PCI_CAP_VENDOR_CFG */

begin_packed_struct struct virtio_pci_vndr_data_s
{
    uint8_t cap_vndr;   /* Generic PCI field: PCI_CAP_ID_VNDR */
    uint8_t cap_next;   /* Generic PCI field: next ptr. */
    uint8_t cao_len;    /* Generic PCI field: capability length */
    uint8_t cfg_type;   /* Identifies the structure. */
    uint16_t vendor_id; /* Identifies the vendor-specific format. */

    /* For Vendor Definition
     * Pads structure to a multiple of 4 bytes
     * Reads must not have side effects
     */
} end_packed_struct;

/* VIRTIO_PCI_CAP_PCI_CFG */

begin_packed_struct struct virtio_pci_cfg_cap_s
{
    struct virtio_pci_cap_s cap;
    uint8_t pci_cfg_data[4]; /* Data for BAR access. */
} end_packed_struct;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/* Helper functions */

static uint16_t virtio_pci_modern_get_queue_len(struct virtio_pci_device_s *vpdev, int idx);
static int virtio_pci_modern_config_vector(struct virtio_pci_device_s *vpdev, bool enable);
static int virtio_pci_modern_create_virtqueue(struct virtio_pci_device_s *vpdev,
                                              struct virtqueue *vq);
static void virtio_pci_modern_delete_virtqueue(struct virtio_device *vdev, int index);
static void virtio_pci_modern_set_status(struct virtio_device *vdev, uint8_t status);
static uint8_t virtio_pci_modern_get_status(struct virtio_device *vdev);
static void virtio_pci_modern_write_config(struct virtio_device *vdev, uint32_t offset, void *dst,
                                           int length);
static void virtio_pci_modern_read_config(struct virtio_device *vdev, uint32_t offset, void *dst,
                                          int length);
static uint64_t virtio_pci_modern_get_features(struct virtio_device *vdev);
static void virtio_pci_modern_set_features(struct virtio_device *vdev, uint64_t features);
static void virtio_pci_modern_notify(struct virtqueue *vq);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct virtio_dispatch g_virtio_pci_dispatch = {
    virtio_pci_create_virtqueues,   /* create_virtqueues */
    virtio_pci_delete_virtqueues,   /* delete_virtqueues */
    virtio_pci_modern_get_status,   /* get_status */
    virtio_pci_modern_set_status,   /* set_status */
    virtio_pci_modern_get_features, /* get_features */
    virtio_pci_modern_set_features, /* set_features */
    virtio_pci_negotiate_features,  /* negotiate_features */
    virtio_pci_modern_read_config,  /* read_config */
    virtio_pci_modern_write_config, /* write_config */
    virtio_pci_reset_device,        /* reset_device */
    virtio_pci_modern_notify,       /* notify */
};

static const struct virtio_pci_ops_s g_virtio_pci_modern_ops = {
    virtio_pci_modern_get_queue_len,    /* get_queue_len */
    virtio_pci_modern_config_vector,    /* config_vector */
    virtio_pci_modern_create_virtqueue, /* create_virtqueue */
    virtio_pci_modern_delete_virtqueue, /* delete_virtqueue */
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: virtio_pci_cap_find
 ****************************************************************************/
uint8_t pci_find_next_capability(struct pci_dev *dev, uint8_t pos, int cap)
{
    uint8_t next;
    uint8_t cap_vndr;

    while (1)
    {
        pci_read_config_byte(dev, pos + offsetof(struct virtio_pci_cap_s, cap_next), &next);
        if (!next)
        {
            return 0;
        }

        pci_read_config_byte(dev, next + offsetof(struct virtio_pci_cap_s, cap_vndr), &cap_vndr);
        if (cap == cap_vndr)
        {
            return next;
        }
        pos = next;
    }
}

static uint8_t virtio_pci_cap_find(struct pci_dev *dev, uint8_t cfg_type, unsigned int flags)
{
    uint8_t type;
    uint8_t bar;
    uint8_t pos;

    for (pos = pci_cap_find(dev, PCI_CAP_ID_VNDR); pos > 0;
         pos = pci_find_next_capability(dev, pos, PCI_CAP_ID_VNDR))
    {
        pci_read_config_byte(dev, pos + offsetof(struct virtio_pci_cap_s, cfg_type), &type);
        pci_read_config_byte(dev, pos + offsetof(struct virtio_pci_cap_s, bar), &bar);

        /* VirtIO Spec v1.2: The driver MUST ignore any vendor-specific
         * capability structure which has a reserved cfg_type value.
         */

        if (bar >= PCI_STD_NUM_BARS)
        {
            continue;
        }

        // if (type == cfg_type && pci_resource_len(dev, bar) &&
        //   (pci_resource_flags(dev, bar) & flags))

        if (type == cfg_type && pci_resource_len(dev, bar))
        {
            return pos;
        }
    }

    return 0;
}

/****************************************************************************
 * Name: virtio_pci_map_capability
 ****************************************************************************/

static void *virtio_pci_map_capability(struct virtio_pci_device_s *vpdev, uint8_t pos, size_t *len)
{
    struct pci_dev *dev = vpdev->dev;
    void *ptr;
    uint32_t offset;
    uint32_t length;
    uint8_t bar;

    pci_read_config_byte(dev, pos + offsetof(struct virtio_pci_cap_s, bar), &bar);
    pci_read_config_dword(dev, pos + offsetof(struct virtio_pci_cap_s, offset), &offset);
    pci_read_config_dword(dev, pos + offsetof(struct virtio_pci_cap_s, length), &length);

    if (bar >= PCI_STD_NUM_BARS)
    {
        printk("Bar error bard=%u\n", bar);
        return NULL;
    }

    ptr = (void *)dev->bar[bar].addr + offset;
    ptr = (void *)ioremap((phys_addr_t)ptr, (size_t)length);

    if (len)
    {
        *len = length;
    }

    return ptr;
}

/****************************************************************************
 * Name: virtio_pci_modern_get_queue_len
 ****************************************************************************/

static uint16_t virtio_pci_modern_get_queue_len(struct virtio_pci_device_s *vpdev, int idx)
{
    struct virtio_pci_common_cfg_s *cfg = vpdev->common;
    uint16_t size;

    pci_write_io_word(vpdev->dev, (uintptr_t)&cfg->queue_select, idx);
    pci_read_io_word(vpdev->dev, (uintptr_t)&cfg->queue_size, &size);
    return size;
}

/****************************************************************************
 * Name: virtio_pci_modern_create_virtqueue
 ****************************************************************************/

static int virtio_pci_modern_create_virtqueue(struct virtio_pci_device_s *vpdev,
                                              struct virtqueue *vq)
{
    struct virtio_pci_common_cfg_s *cfg = vpdev->common;
    struct virtio_vring_info *vrinfo;
#if CONFIG_DRIVERS_VIRTIO_PCI_POLLING_PERIOD <= 0
    uint16_t msix_vector;
#endif
    uint16_t off;

    /* Activate the queue */

    pci_write_io_word(vpdev->dev, (uintptr_t)&cfg->queue_select, vq->vq_queue_index);
    pci_write_io_word(vpdev->dev, (uintptr_t)&cfg->queue_size, vq->vq_ring.num);

    pci_write_io_qword(vpdev->dev, (uintptr_t)&cfg->queue_desc,
                       mm_kernel_v2p((virt_addr_t)(long)vq->vq_ring.desc));
    pci_write_io_qword(vpdev->dev, (uintptr_t)&cfg->queue_avail,
                       mm_kernel_v2p((virt_addr_t)vq->vq_ring.avail));
    pci_write_io_qword(vpdev->dev, (uintptr_t)&cfg->queue_used,
                       mm_kernel_v2p((virt_addr_t)vq->vq_ring.used));

#if CONFIG_DRIVERS_VIRTIO_PCI_POLLING_PERIOD <= 0

    /* 设置msi-x entry号为VIRTIO_PCI_INT_VQ */
    pci_write_io_word(vpdev->dev, (uintptr_t)&cfg->queue_msix_vector, VIRTIO_PCI_INT_VQ);
    pci_read_io_word(vpdev->dev, (uintptr_t)&cfg->queue_msix_vector, &msix_vector);
    if (msix_vector == VIRTIO_PCI_MSI_NO_VECTOR)
    {
        printk("Msix_vector is no vector\n");
        return -EBUSY;
    }
#endif

    /* Enable vq */

    pci_write_io_word(vpdev->dev, (uintptr_t)&cfg->queue_enable, 1);

    pci_write_io_word(vpdev->dev, (uintptr_t)&cfg->queue_select, vq->vq_queue_index);
    pci_read_io_word(vpdev->dev, (uintptr_t)&cfg->queue_notify_off, &off);

    if (off * vpdev->notify_off_multiplier + 2 > vpdev->notify_len)
    {
        return -EINVAL;
    }

    vrinfo = &vpdev->vdev.vrings_info[vq->vq_queue_index];
    vrinfo->notifyid = off * vpdev->notify_off_multiplier;

    return 0;
}

/****************************************************************************
 * Name: virtio_pci_modern_config_vector
 ****************************************************************************/

static int virtio_pci_modern_config_vector(struct virtio_pci_device_s *vpdev, bool enable)
{
    struct virtio_pci_common_cfg_s *cfg = vpdev->common;
    uint16_t vector = enable ? 0 : VIRTIO_PCI_MSI_NO_VECTOR;
    uint16_t rvector;

    pci_write_io_word(vpdev->dev, (uintptr_t)&cfg->config_msix_vector, vector);
    pci_read_io_word(vpdev->dev, (uintptr_t)&cfg->config_msix_vector, &rvector);
    if (rvector != vector)
    {
        return -EINVAL;
    }

    return 0;
}

/****************************************************************************
 * Name: virtio_pci_modern_delete_virtqueue
 ****************************************************************************/

void virtio_pci_modern_delete_virtqueue(struct virtio_device *vdev, int index)
{
    struct virtio_pci_device_s *vpdev = (struct virtio_pci_device_s *)vdev;
    struct virtio_pci_common_cfg_s *cfg = vpdev->common;

    pci_write_io_word(vpdev->dev, (uintptr_t)&cfg->queue_select, index);

#if CONFIG_DRIVERS_VIRTIO_PCI_POLLING_PERIOD <= 0
    pci_write_io_word(vpdev->dev, (uintptr_t)&cfg->queue_msix_vector, VIRTIO_PCI_MSI_NO_VECTOR);
#endif
}

/****************************************************************************
 * Name: virtio_pci_modern_set_status
 ****************************************************************************/

static void virtio_pci_modern_set_status(struct virtio_device *vdev, uint8_t status)
{
    struct virtio_pci_device_s *vpdev = (struct virtio_pci_device_s *)vdev;
    struct virtio_pci_common_cfg_s *cfg = vpdev->common;

    pci_write_io_byte(vpdev->dev, (uintptr_t)&cfg->device_status, status);
}

/****************************************************************************
 * Name: virtio_pci_modern_get_status
 ****************************************************************************/

static uint8_t virtio_pci_modern_get_status(struct virtio_device *vdev)
{
    struct virtio_pci_device_s *vpdev = (struct virtio_pci_device_s *)vdev;
    struct virtio_pci_common_cfg_s *cfg = vpdev->common;
    uint8_t status;

    pci_read_io_byte(vpdev->dev, (uintptr_t)&cfg->device_status, &status);
    return status;
}

/****************************************************************************
 * Name: virtio_pci_modern_write_config
 ****************************************************************************/

static void virtio_pci_modern_write_config(struct virtio_device *vdev, uint32_t offset, void *src,
                                           int length)
{
    struct virtio_pci_device_s *vpdev = (struct virtio_pci_device_s *)vdev;
    uint64_t u64data;
    uint32_t u32data;
    uint16_t u16data;
    uint8_t u8data;

    if (offset + length > vpdev->device_len)
    {
        return;
    }

    switch (length)
    {
    case 1:
        memcpy(&u8data, src, sizeof(u8data));
        pci_write_io_byte(vpdev->dev, (uintptr_t)(vpdev->device + offset), u8data);
        break;
    case 2:
        memcpy(&u16data, src, sizeof(u16data));
        pci_write_io_word(vpdev->dev, (uintptr_t)(vpdev->device + offset), u16data);
        break;
    case 4:
        memcpy(&u32data, src, sizeof(u32data));
        pci_write_io_dword(vpdev->dev, (uintptr_t)(vpdev->device + offset), u32data);
        break;
    case 8:
        memcpy(&u64data, src, sizeof(u64data));
        pci_write_io_qword(vpdev->dev, (uintptr_t)(vpdev->device + offset), u64data);
        break;
    default:
    {
        char *s = src;
        int i;
        for (i = 0; i < length; i++)
        {
            pci_write_io_byte(vpdev->dev, (uintptr_t)(vpdev->device + offset + i), s[i]);
        }
    }
    }
}

/****************************************************************************
 * Name: virtio_pci_modern_read_config
 ****************************************************************************/

static void virtio_pci_modern_read_config(struct virtio_device *vdev, uint32_t offset, void *dst,
                                          int length)
{
    struct virtio_pci_device_s *vpdev = (struct virtio_pci_device_s *)vdev;
    uint64_t u64data;
    uint32_t u32data;
    uint16_t u16data;
    uint8_t u8data;

    if (offset + length > vpdev->device_len)
    {
        return;
    }

    switch (length)
    {
    case 1:
        pci_read_io_byte(vpdev->dev, (uintptr_t)(vpdev->device + offset), &u8data);
        memcpy(dst, &u8data, sizeof(u8data));
        break;
    case 2:
        pci_read_io_word(vpdev->dev, (uintptr_t)(vpdev->device + offset), &u16data);
        memcpy(dst, &u16data, sizeof(u16data));
        break;
    case 4:
        pci_read_io_dword(vpdev->dev, (uintptr_t)(vpdev->device + offset), &u32data);
        memcpy(dst, &u32data, sizeof(u32data));
        break;
    case 8:
        pci_read_io_qword(vpdev->dev, (uintptr_t)(vpdev->device + offset), &u64data);
        memcpy(dst, &u64data, sizeof(u64data));
        break;
    default:
    {
        char *d = dst;
        int i;
        for (i = 0; i < length; i++)
        {
            pci_read_io_byte(vpdev->dev, (uintptr_t)(vpdev->device + offset + i), (uint8_t *)&d[i]);
        }
    }
    }
}

/****************************************************************************
 * Name: virtio_pci_modern_get_features
 ****************************************************************************/

static uint64_t virtio_pci_modern_get_features(struct virtio_device *vdev)
{
    struct virtio_pci_device_s *vpdev = (struct virtio_pci_device_s *)vdev;
    struct virtio_pci_common_cfg_s *cfg = vpdev->common;
    uint32_t feature_lo;
    uint32_t feature_hi;

    pci_write_io_dword(vpdev->dev, (uintptr_t)&cfg->device_feature_select, 0);
    pci_read_io_dword(vpdev->dev, (uintptr_t)&cfg->device_feature, &feature_lo);
    pci_write_io_dword(vpdev->dev, (uintptr_t)&cfg->device_feature_select, 1);
    pci_read_io_dword(vpdev->dev, (uintptr_t)&cfg->device_feature, &feature_hi);
    return ((uint64_t)feature_hi << 32) | (uint64_t)feature_lo;
}

/****************************************************************************
 * Name: virtio_pci_modern_set_features
 ****************************************************************************/

static void virtio_pci_modern_set_features(struct virtio_device *vdev, uint64_t features)
{
    struct virtio_pci_device_s *vpdev = (struct virtio_pci_device_s *)vdev;
    struct virtio_pci_common_cfg_s *cfg = vpdev->common;

    pci_write_io_dword(vpdev->dev, (uintptr_t)&cfg->driver_feature_select, 0);
    pci_write_io_dword(vpdev->dev, (uintptr_t)&cfg->driver_feature, features);
    pci_write_io_dword(vpdev->dev, (uintptr_t)&cfg->driver_feature_select, 1);
    pci_write_io_dword(vpdev->dev, (uintptr_t)&cfg->driver_feature, features >> 32);
    vdev->features = features;
}

/****************************************************************************
 * Name: virtio_pci_modern_notify
 ****************************************************************************/

static void virtio_pci_modern_notify(struct virtqueue *vq)
{
    struct virtio_pci_device_s *vpdev = (struct virtio_pci_device_s *)vq->vq_dev;
    struct virtio_vring_info *vrinfo;

    vrinfo = &vpdev->vdev.vrings_info[vq->vq_queue_index];

    pci_write_io_word(vpdev->dev, (uintptr_t)(vpdev->notify + vrinfo->notifyid),
                      vq->vq_queue_index);
}

/****************************************************************************
 * Name: virtio_pci_probe
 ****************************************************************************/

static int virtio_pci_init_device(struct virtio_pci_device_s *vpdev)
{
    struct virtio_device *vdev = &vpdev->vdev;
    struct pci_dev *dev = vpdev->dev;
    uint8_t common;
    uint8_t notify;
    uint8_t device;
    int8_t ret = 0;

    if (dev->device < 0x1040)
    {
        /* Transitional devices: use the PCI subsystem device id as
         * virtio device id, same as legacy driver always did.
         */

        vdev->id.device = dev->subsystem_device;
    }
    else
    {
        /* Modern devices: simply use PCI device id, but start from 0x1040. */

        vdev->id.device = dev->device - 0x1040;
    }

    vdev->id.vendor = dev->subsystem_vendor;

    /* Find pci capabilities */

    common = virtio_pci_cap_find(dev, VIRTIO_PCI_CAP_COMMON_CFG, PCI_RESOURCE_MEM);
    if (common)
    {
        vpdev->common = virtio_pci_map_capability(vpdev, common, &vpdev->common_len);
    }
    else
    {
        return -ENODEV;
    }

    notify = virtio_pci_cap_find(dev, VIRTIO_PCI_CAP_NOTIFY_CFG, PCI_RESOURCE_MEM);
    if (notify)
    {
        pci_read_config_dword(
            dev, notify + offsetof(struct virtio_pci_notify_cap_s, notify_off_multiplier),
            &vpdev->notify_off_multiplier);

        vpdev->notify = virtio_pci_map_capability(vpdev, notify, &vpdev->notify_len);
    }

    device = virtio_pci_cap_find(dev, VIRTIO_PCI_CAP_DEVICE_CFG, PCI_RESOURCE_MEM);
    if (device != 0)
    {
        vpdev->device = virtio_pci_map_capability(vpdev, device, &vpdev->device_len);
    }

    return ret;
}

/****************************************************************************
 * Name: virtio_pci_modern_probe
 ****************************************************************************/

int virtio_pci_modern_probe(struct pci_dev *dev)
{
    struct virtio_pci_device_s *vpdev = dev->priv;
    struct virtio_device *vdev = &vpdev->vdev;
    int ret;

    /* We only own devices >= 0x1000 and <= 0x107f: leave the rest. */

    if (dev->device < 0x1000 || dev->device > 0x107f)
    {
        return -ENODEV;
    }

    vpdev->ops = &g_virtio_pci_modern_ops;
    vdev->func = &g_virtio_pci_dispatch;
    vdev->role = VIRTIO_DEV_DRIVER;

    ret = virtio_pci_init_device(vpdev);
    if (ret < 0)
    {
        if (ret != -ENODEV)
        {
            printk("Virtio pci modern device init failed, ret=%d\n", ret);
        }
    }

    return ret;
}
