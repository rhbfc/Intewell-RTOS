/****************************************************************************
 * include/nuttx/virtio/virtio.h
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

#ifndef __INCLUDE_NUTTX_VIRTIO_VIRTIO_H
#define __INCLUDE_NUTTX_VIRTIO_VIRTIO_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "virtio_helper.h"
#include <driver/driver.h>
#include <list.h>
#include <spinlock.h>
#include <stdint.h>
#include <system/compiler.h>

#ifdef CONFIG_DRIVERS_VIRTIO

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define virtio_has_feature(vdev, fbit) (((vdev)->features & (1UL << (fbit))) != 0)

#define virtio_read_config_member(vdev, structname, member, ptr)                                   \
    virtio_read_config((vdev), offsetof(structname, member), (ptr), sizeof(*(ptr)));

#define virtio_write_config_member(vdev, structname, member, ptr)                                  \
    virtio_write_config((vdev), offsetof(structname, member), (ptr), sizeof(*(ptr)));

/****************************************************************************
 * Public Type Definitions
 ****************************************************************************/

struct virtio_driver
{
    struct driver driver;
    int (*probe)(struct virtio_device *);
    void (*remove)(struct virtio_device *);

    uint32_t device; /* device id */
};

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef __cplusplus
#define EXTERN extern "C"
extern "C" {
#else
#define EXTERN extern
#endif

void *virtqueue_get_buffer(struct virtqueue *vq, uint32_t *len, uint16_t *idx);

static inline void virtqueue_disable_cb_lock(struct virtqueue *vq, ttos_spinlock_t *lock)
{
    irq_flags_t flags;

    spin_lock_irqsave(lock, flags);
    virtqueue_disable_cb(vq);
    spin_unlock_irqrestore(lock, flags);
}

static inline int virtqueue_enable_cb_lock(struct virtqueue *vq, ttos_spinlock_t *lock)
{
    irq_flags_t flags;
    int ret;

    spin_lock_irqsave(lock, flags);
    ret = virtqueue_enable_cb(vq);
    spin_unlock_irqrestore(lock, flags);

    return ret;
}

static inline int virtqueue_add_buffer_lock(struct virtqueue *vq, struct virtqueue_buf *buf_list,
                                            int readable, int writable, void *cookie, ttos_spinlock_t *lock)
{
    irq_flags_t flags;
    int ret;

    spin_lock_irqsave(lock, flags);
    ret = virtqueue_add_buffer(vq, buf_list, readable, writable, cookie);
    spin_unlock_irqrestore(lock, flags);

    return ret;
}

static inline void *virtqueue_get_buffer_lock(struct virtqueue *vq, uint32_t *len,
                                              uint16_t *idx, ttos_spinlock_t *lock)
{
    irq_flags_t flags;
    void *ret;

    spin_lock_irqsave(lock, flags);
    ret = virtqueue_get_buffer(vq, len, idx);
    spin_unlock_irqrestore(lock, flags);

    return ret;
}

static inline void virtqueue_kick_lock(struct virtqueue *vq, ttos_spinlock_t *lock)
{
    irq_flags_t flags;

    spin_lock_irqsave(lock, flags);
    virtqueue_kick(vq);
    spin_unlock_irqrestore(lock, flags);
}

/* Driver and device register/unregister function */

int virtio_add_driver(struct virtio_driver *driver);

void *virtio_alloc_buf(struct virtio_device *vdev, size_t size, size_t align);
void *virtio_zalloc_buf(struct virtio_device *vdev, size_t size, size_t align);
void virtio_free_buf(struct virtio_device *vdev, void *buf);

int virtio_device_register(struct virtio_device *device);

extern struct bus_type virtio_bus_type;

#define dev_is_virtio(dev) ((dev)->bus == &virtio_bus_type)
#define to_virtio_device(x) container_of((x), struct virtio_device, device)

#define to_virtio_driver(drv) (container_of((drv), struct virtio_driver, driver))

#undef EXTERN
#ifdef __cplusplus
}
#endif

#endif /* CONFIG_DRIVERS_VIRTIO */

#endif /* __INCLUDE_NUTTX_VIRTIO_VIRTIO_H */
