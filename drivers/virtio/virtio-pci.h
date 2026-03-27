/****************************************************************************
 * drivers/virtio/virtio-pci.h
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

#ifndef __DRIVERS_VIRTIO_VIRTIO_PCI_H
#define __DRIVERS_VIRTIO_VIRTIO_PCI_H

/****************************************************************************
 * Included Files
 ****************************************************************************/
#ifdef __x86_64__
#include <asm/io.h>
#endif
#include <driver/pci/pci.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <system/types.h>

#include "virtio_helper.h"
//#ifdef CONFIG_DRIVERS_VIRTIO_PCI

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
#define CONFIG_DRIVERS_VIRTIO_PCI_POLLING_PERIOD 0

#define VIRTIO_PCI_VRING_ALIGN (1 << 12)

#define VIRTIO_PCI_MSI_NO_VECTOR 0xffff

#define VIRTIO_PCI_INT_CFG 0
#define VIRTIO_PCI_INT_VQ 1
#define VIRTIO_PCI_INT_NUM 2

#define VIRTIO_PCI_INT_CFG_IRQ 50
#define VIRTIO_PCI_INT_VQ_IRQ 51

/****************************************************************************
 * Public Data
 ****************************************************************************/

#ifdef __cplusplus
#define EXTERN extern "C"
extern "C" {
#else
#define EXTERN extern
#endif

/****************************************************************************
 * Public Types
 ****************************************************************************/

#define begin_packed_struct
#define end_packed_struct __attribute__((packed))

struct virtio_pci_device_s;
struct virtio_pci_ops_s
{
    uint16_t (*get_queue_len)(struct virtio_pci_device_s *vpdev, int idx);
    int (*config_vector)(struct virtio_pci_device_s *vpdev, bool enable);
    int (*create_virtqueue)(struct virtio_pci_device_s *vpdev, struct virtqueue *vq);
    void (*delete_virtqueue)(struct virtio_device *vdev, int index);
};

struct virtio_pci_device_s
{
    struct virtio_device vdev; /* Virtio deivce */
    struct pci_dev *dev;       /* PCI device */
    const struct virtio_pci_ops_s *ops;
    phys_addr_t shm_phy;
    /*struct metal_io_region             shm_io;  */
    /* Share memory io region,
     * virtqueue use this io.
     */
#if CONFIG_DRIVERS_VIRTIO_PCI_POLLING_PERIOD <= 0
    int irq[VIRTIO_PCI_INT_NUM];
#else
    struct wdog_s wdog;
#endif

    /* for modern */

    void *common;
    size_t common_len;
    void *notify;
    size_t notify_len;
    uint32_t notify_off_multiplier;
    void *device;
    size_t device_len;

    /* for legacy */

    void *ioaddr;
};

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

void virtio_pci_reset_device(struct virtio_device *vdev);
uint64_t virtio_pci_negotiate_features(struct virtio_device *vdev, uint64_t features);
int virtio_pci_create_virtqueues(struct virtio_device *vdev, unsigned int flags, unsigned int nvqs,
                                 const char *names[], vq_callback callbacks[]);
void virtio_pci_delete_virtqueues(struct virtio_device *vdev);

int virtio_pci_modern_probe(struct pci_dev *dev);
int virtio_pci_legacy_probe(struct pci_dev *dev);

#undef EXTERN
#ifdef __cplusplus
}
#endif

//#endif /* CONFIG_DRIVERS_VIRTIO_PCI */

#endif /* __DRIVERS_VIRTIO_VIRTIO_PCI_H */
