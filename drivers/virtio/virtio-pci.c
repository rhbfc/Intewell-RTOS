/****************************************************************************
 * drivers/virtio/virtio-pci.c
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

#include "virtio-pci.h"
#include "virtio.h"
#include "virtio_helper.h"
#include <driver/class/chardev.h>
#include <driver/device.h>
#include <driver/pci/pci.h>
#include <errno.h>
#include <fs/fs.h>
#include <fs/ioctl.h>
#include <fs/kpoll.h>
#include <io.h>
#include <kmalloc.h>
#include <stddef.h>
#include <stdio.h>
#include <ttosProcess.h>
#include <ttos_init.h>
#include <ttos_pic.h>
#include <uaccess.h>

#define KLOG_TAG "VIRTIO-PCI"
#include <klog.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define VIRTIO_PCI_WORK_DELAY USEC2TICK(CONFIG_DRIVERS_VIRTIO_PCI_POLLING_PERIOD)

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static void *virtio_pci_alloc_buf(struct virtio_device *vdev, size_t size, size_t align);
static void virtio_pci_free_buf(struct virtio_device *vdev, void *buf);
static void virtio_pci_remove(struct pci_dev *dev);

static int virtio_pci_create_virtqueue(struct virtio_pci_device_s *vpdev, unsigned int i,
                                       const char *name, vq_callback callback);

/****************************************************************************
 * Private Data
 ****************************************************************************/

struct virtio_memory_ops
{
    void *(*alloc)(struct virtio_device *dev, size_t size, size_t align);
    void (*free)(struct virtio_device *dev, void *buf);
};

static const struct pci_device_id g_virtio_pci_id_table[] = {
    {PCI_DEVICE(PCI_VENDOR_ID_REDHAT_QUMRANET, 0x1001)},
    {PCI_DEVICE(PCI_VENDOR_ID_REDHAT_QUMRANET, 0x1041)},
    {PCI_DEVICE(PCI_VENDOR_ID_REDHAT_QUMRANET, 0x1042)},
    {PCI_DEVICE(PCI_VENDOR_ID_REDHAT_QUMRANET, 0x1000)},
    {
        0,
    }};

static const struct virtio_memory_ops g_virtio_pci_mmops = {
    virtio_pci_alloc_buf, /* Alloc */
    virtio_pci_free_buf,  /* Free */
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: virtio_pci_vq_callback
 ****************************************************************************/

static void virtio_pci_vq_callback(struct virtio_pci_device_s *vpdev)
{
    struct virtio_vring_info *vrings_info = vpdev->vdev.vrings_info;
    struct virtqueue *vq;
    unsigned int i;

    for (i = 0; i < vpdev->vdev.vrings_num; i++)
    {
        vq = vrings_info[i].vq;
        if (vq->vq_used_cons_idx != vq->vq_ring.used->idx && vq->callback != NULL)
        {
            vq->callback(vq);
        }
    }
}

#if CONFIG_DRIVERS_VIRTIO_PCI_POLLING_PERIOD <= 0
/****************************************************************************
 * Name: virtio_pci_interrupt
 ****************************************************************************/

static void virtio_pci_interrupt(uint32_t irq, void *arg)
{
    struct virtio_pci_device_s *vpdev = arg;

    virtio_pci_vq_callback(vpdev);
}

/****************************************************************************
 * Name: virtio_pci_config_changed
 ****************************************************************************/
static void virtio_pci_config_changed(uint32_t irq, void *arg)
{
    /* TODO: not support config changed notification */

    return;
}

#else
/****************************************************************************
 * Name: virtio_pci_wdog
 ****************************************************************************/

static void virtio_pci_wdog(wdparm_t arg)
{
    struct virtio_pci_device_s *vpdev = (struct virtio_pci_device_s *)arg;

    virtio_pci_vq_callback(vpdev);
    wd_start(&vpdev->wdog, VIRTIO_PCI_WORK_DELAY, virtio_pci_wdog, (wdparm_t)vpdev);
}
#endif

/****************************************************************************
 * Name: virtio_pci_alloc_buf
 ****************************************************************************/

static void *virtio_pci_alloc_buf(struct virtio_device *vdev, size_t size, size_t align)
{
    return memalign(align, size);
}

/****************************************************************************
 * Name: virtio_pci_free_buf
 ****************************************************************************/

static void virtio_pci_free_buf(struct virtio_device *vdev, void *buf)
{
    return free(buf);
}

/****************************************************************************
 * Name: virtio_pci_probe
 ****************************************************************************/
static int virtio_pci_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
    struct virtio_pci_device_s *vpdev;
    struct virtio_device *vdev;
    int ret;

    /* We only own devices >= 0x1000 and <= 0x107f: leave the rest. */
    if (dev->device < 0x1000 || dev->device > 0x107f)
    {
        KLOG_EMERG("Pci device id err, id=%d", dev->device);
        return -ENODEV;
    }

    /* 使用托管内存分配 */
    vpdev = devm_kzalloc(&dev->dev, sizeof(*vpdev), GFP_KERNEL);
    if (vpdev == NULL)
    {
        printk("No enough memory\n");
        return -ENOMEM;
    }

    dev->priv = vpdev;
    vpdev->dev = dev;
    vdev = &vpdev->vdev;

    ret = virtio_pci_modern_probe(dev);
    if (ret == -ENODEV)
    {
        ret = virtio_pci_legacy_probe(dev);
        if (ret < 0)
        {
            printk("Virtio pci legacy probe failed\n");
            return ret;
        }
    }

    vdev->device.of_node = (struct device_node *)1; //无实际意义，仅为兼容

    pci_irq_alloc(vpdev->dev, PCI_INT_MSIX, 3);

    /* Irq init（PCI 中断已由 pci_irq_install 托管） */
    vpdev->irq[VIRTIO_PCI_INT_CFG] = vpdev->dev->msi[VIRTIO_PCI_INT_CFG].irq;
    vpdev->irq[VIRTIO_PCI_INT_VQ] = vpdev->dev->msi[VIRTIO_PCI_INT_VQ].irq;

    pci_irq_install(vpdev->irq[VIRTIO_PCI_INT_VQ], virtio_pci_interrupt, vpdev,
                    "VIRTIO_PCI_INT_VQ");
    pci_irq_install(vpdev->irq[VIRTIO_PCI_INT_CFG], virtio_pci_config_changed, vpdev,
                    "VIRTIO_PCI_INT_CFG");

    pci_irq_umask(dev, vpdev->irq[VIRTIO_PCI_INT_VQ]);
    pci_irq_umask(dev, vpdev->irq[VIRTIO_PCI_INT_CFG]);

    virtio_set_status(vdev, VIRTIO_CONFIG_STATUS_RESET);
    virtio_set_status(vdev, VIRTIO_CONFIG_STATUS_ACK);

    virtio_device_register(&vpdev->vdev);

    return 0;
}

/****************************************************************************
 * Name: virtio_pci_remove
 ****************************************************************************/

static void virtio_pci_remove(struct pci_dev *dev)
{
    /* TODO: virtio_unregister_device() if needed */

    /* 所有资源（内存、BAR映射、中断）由 devres/PCI 自动释放 */
}

/****************************************************************************
 * Name: virtio_pci_create_virtqueue
 ****************************************************************************/

static int virtio_pci_create_virtqueue(struct virtio_pci_device_s *vpdev, unsigned int i,
                                       const char *name, vq_callback callback)
{
    struct virtio_device *vdev = &vpdev->vdev;
    struct virtio_vring_info *vrinfo;
    struct vring_alloc_info *vralloc;
    struct virtqueue *vq;
    int vringsize;
    int ret;

    /* Alloc virtqueue and init the vring info and vring alloc info */

    vrinfo = &vdev->vrings_info[i];
    vralloc = &vrinfo->info;
    vralloc->num_descs = vpdev->ops->get_queue_len(vpdev, i);
    vq = virtqueue_allocate(vralloc->num_descs);
    if (vq == NULL)
    {
        printk("Virtqueue_allocate failed\n");
        return -ENOMEM;
    }

    /* Init the vring info and vring alloc info */

    vrinfo->vq = vq;
    // vrinfo->io = &vpdev->shm_io;
    vralloc->align = VIRTIO_PCI_VRING_ALIGN;
    vringsize = vring_size(vralloc->num_descs, VIRTIO_PCI_VRING_ALIGN);
    vralloc->vaddr = virtio_zalloc_buf(vdev, vringsize, VIRTIO_PCI_VRING_ALIGN);
    if (vralloc->vaddr == NULL)
    {
        printk("Vring alloc failed\n");
        return -ENOMEM;
    }

    /* Initialize the virtio queue */

    ret = virtqueue_create(vdev, i, name, vralloc, callback, vdev->func->notify, vq);
    if (ret < 0)
    {
        printk("Virtqueue create error, ret=%d\n", ret);
        return ret;
    }

    /* Set the pci virtqueue register, active vq, enable vq */

    ret = vpdev->ops->create_virtqueue(vpdev, vq);
    if (ret < 0)
    {
        printk("Virtio_pci_config_virtqueue failed, ret=%d\n", ret);
        return ret;
    }

    return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: virtio_pci_create_virtqueues
 ****************************************************************************/

int virtio_pci_create_virtqueues(struct virtio_device *vdev, unsigned int flags, unsigned int nvqs,
                                 const char *names[], vq_callback callbacks[])
{
    struct virtio_pci_device_s *vpdev = (struct virtio_pci_device_s *)vdev;
    unsigned int i;
    int ret = 0;

    /* 使用托管内存分配 vring info */
    vdev->vrings_num = nvqs;
    vdev->vrings_info =
        devm_kzalloc(&vpdev->dev->dev, sizeof(struct virtio_vring_info) * nvqs, GFP_KERNEL);
    if (vdev->vrings_info == NULL)
    {
        printk("Alloc vrings info failed\n");
        return -ENOMEM;
    }

    /* Alloc and init the virtqueue */

    for (i = 0; i < nvqs; i++)
    {
        ret = virtio_pci_create_virtqueue(vpdev, i, names[i], callbacks[i]);
        if (ret < 0)
        {
            goto err;
        }
    }

#if CONFIG_DRIVERS_VIRTIO_PCI_POLLING_PERIOD <= 0
    ret = vpdev->ops->config_vector(vpdev, true);
    if (ret < 0)
    {
        printk("Config vector failed, vector=%u\n", ret);
        goto err;
    }

    ttos_pic_irq_unmask(vpdev->irq[VIRTIO_PCI_INT_CFG]);
    ttos_pic_irq_unmask(vpdev->irq[VIRTIO_PCI_INT_VQ]);
#else
    ret = wd_start(&vpdev->wdog, VIRTIO_PCI_WORK_DELAY, virtio_pci_wdog, (wdparm_t)vpdev);
    if (ret < 0)
    {
        printk("Wd_start failed: %d\n", ret);
        goto err;
    }
#endif

    return ret;

err:
    virtio_pci_delete_virtqueues(vdev);
    return ret;
}

/****************************************************************************
 * Name: virtio_pci_delete_virtqueues
 ****************************************************************************/

void virtio_pci_delete_virtqueues(struct virtio_device *vdev)
{
    //   struct virtio_pci_device_s *vpdev =
    //     (struct virtio_pci_device_s *)vdev;
    //   struct virtio_vring_info *vrinfo;
    //   unsigned int i;

    // #if CONFIG_DRIVERS_VIRTIO_PCI_POLLING_PERIOD <= 0
    //   ttos_pic_irq_mask(vpdev->irq[VIRTIO_PCI_INT_CFG]);
    //   ttos_pic_irq_mask(vpdev->irq[VIRTIO_PCI_INT_VQ]);
    //   vpdev->ops->config_vector(vpdev, false);
    // #else
    //   wd_cancel(&vpdev->wdog);
    // #endif

    //   /* Free the memory */

    //   if (vdev->vrings_info != NULL)
    //     {
    //       for (i = 0; i < vdev->vrings_num; i++)
    //         {
    //           vrinfo = &vdev->vrings_info[i];
    //           vpdev->ops->delete_virtqueue(vdev, i);

    //           /* Free the vring buffer and virtqueue */

    //           if (vrinfo->info.vaddr != NULL)
    //             {
    //               virtio_free_buf(vdev, vrinfo->info.vaddr);
    //             }

    //           if (vrinfo->vq != NULL)
    //             {
    //               virtqueue_free(vrinfo->vq);
    //             }
    //         }

    //       free(vdev->vrings_info);
    //     }
}

/****************************************************************************
 * Name: virtio_pci_negotiate_features
 ****************************************************************************/

uint64_t virtio_pci_negotiate_features(struct virtio_device *vdev, uint64_t features)
{
    features = features & vdev->func->get_features(vdev);
    vdev->func->set_features(vdev, features);
    return features;
}

/****************************************************************************
 * Name: virtio_pci_reset_device
 ****************************************************************************/

void virtio_pci_reset_device(struct virtio_device *vdev)
{
    vdev->func->set_features(vdev, VIRTIO_CONFIG_STATUS_RESET);
}

/****************************************************************************
 * Name: register_virtio_pci_driver
 ****************************************************************************/
static struct pci_driver virtio_pci_driver = {
    .name = "virtio-pci",
    .id_table = g_virtio_pci_id_table,
    .probe = virtio_pci_probe,
    .remove = virtio_pci_remove,
};

PCI_DEV_DRIVER_EXPORT(virtio_pci_driver);
