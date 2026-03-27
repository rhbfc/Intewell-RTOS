/****************************************************************************
 * drivers/virtio/virtio-blk.c
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

#include "ttosUtils.h"
#include "virtio.h"
#include <assert.h>
#include <cache.h>
#include <errno.h>
#include <fs/fs.h>
#include <fs/ioctl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <ttos.h>
#include <ttosInterTask.inl>
#include <ttos_init.h>

#define KLOG_TAG "VIRTIO_BLK"
#include <klog.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define VIRTIO_BLK_REQ_HEADER_SIZE sizeof(struct virtio_blk_req_s)
#define VIRTIO_BLK_RESP_HEADER_SIZE sizeof(struct virtio_blk_resp_s)

/* Block feature bits */

#define VIRTIO_BLK_F_RO 5       /* Disk is read-only */
#define VIRTIO_BLK_F_BLK_SIZE 6 /* Block size of disk is available */
#define VIRTIO_BLK_F_FLUSH 9    /* Cache flush command support */

/* Block request type */

#define VIRTIO_BLK_T_IN 0    /* READ */
#define VIRTIO_BLK_T_OUT 1   /* WRITE */
#define VIRTIO_BLK_T_FLUSH 4 /* FLUSH */

/* Block request return status */

#define VIRTIO_BLK_S_OK 0
#define VIRTIO_BLK_S_IOERR 1
#define VIRTIO_BLK_S_UNSUPP 2

/* Block device sector size */

#define VIRTIO_BLK_SECTOR_SIZE 512

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* Block request out header */

struct virtio_blk_req_s
{
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
} __packed;

/* Block request in header */

struct virtio_blk_resp_s
{
    volatile uint8_t status;
} __packed;

struct virtio_blk_config_s
{
    uint64_t capacity;
    uint32_t size_max;
    uint32_t seg_max;
    uint16_t cylinders; /* block geometry */
    uint8_t heads;      /* block geometry */
    uint8_t sectors;    /* block geometry */
    uint32_t blk_size;
    uint8_t physical_block_exp;
    uint8_t alignment_offset;
    uint16_t min_io_size;
    uint32_t opt_io_size;
    uint8_t writeback;
    uint8_t unused0;
    uint16_t num_queues;
    uint32_t max_discard_sectors;
    uint32_t max_discard_seg;
    uint32_t discard_sector_alignment;
    uint32_t max_write_zeroes_sectors;
    uint32_t max_write_zeroes_seg;
    uint8_t write_zeroes_may_unmap;
    uint8_t unused1[3];
    uint32_t max_secure_erase_sectors;
    uint32_t max_secure_erase_seg;
    uint32_t secure_erase_sector_alignment;
} __packed;

struct virtio_blk_priv_s
{
    struct virtio_device *vdev;     /* Virtio deivce */
    struct virtio_blk_req_s *req;   /* Virtio block out header */
    struct virtio_blk_resp_s *resp; /* Virtio block in header */
    ttos_spinlock_t slock;          /* Lock */
    uint64_t nsectors;              /* Sectore numbers */
    char name[NAME_MAX];            /* Device name */
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/* BLK block_operations functions and they helper function */

static ssize_t virtio_blk_rdwr(struct virtio_blk_priv_s *priv, void *buffer, blkcnt_t startsector,
                               unsigned int nsectors, bool write);
static int virtio_blk_open(struct inode *inode);
static int virtio_blk_close(struct inode *inode);
static ssize_t virtio_blk_read(struct inode *inode, unsigned char *buffer, blkcnt_t startsector,
                               unsigned int nsectors);
static ssize_t virtio_blk_write(struct inode *inode, const unsigned char *buffer,
                                blkcnt_t startsector, unsigned int nsectors);
static int virtio_blk_geometry(struct inode *inode, struct geometry *geometry);
static int virtio_blk_ioctl(struct inode *inode, int cmd, unsigned long arg);
static int virtio_blk_flush(struct virtio_blk_priv_s *priv);

/* Other functions */

static int virtio_blk_init(struct virtio_blk_priv_s *priv, struct virtio_device *vdev);
static void virtio_blk_uninit(struct virtio_blk_priv_s *priv);
static void virtio_blk_done(struct virtqueue *vq);
static int virtio_blk_probe(struct virtio_device *vdev);
static void virtio_blk_remove(struct virtio_device *vdev);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct virtio_driver g_virtio_blk_driver = {
    .device = VIRTIO_ID_BLOCK, /* device id */
    .driver.name = "virtblk",
    .probe = virtio_blk_probe,   /* probe */
    .remove = virtio_blk_remove, /* remove */
};

static const struct block_operations g_virtio_blk_bops = {
    virtio_blk_open,     /* open     */
    virtio_blk_close,    /* close    */
    virtio_blk_read,     /* read     */
    virtio_blk_write,    /* write    */
    virtio_blk_geometry, /* geometry */
    virtio_blk_ioctl     /* ioctl    */
};

static int g_virtio_blk_idx = 0;

/****************************************************************************
 * Private Functions
 ****************************************************************************/
static T_TTOS_ReturnCode virtio_blk_wait_complete(struct virtqueue *vq, SEMA_ID respsem)
{
    struct virtio_blk_priv_s *priv = vq->vq_dev->priv;
    SEMA_ID sem;
    T_TTOS_ReturnCode ttos_ret = TTOS_OK;

    if (ttosIsISR() || ttosIsIdleTask(ttosGetRunningTask()))
    {
        for (;;)
        {
            sem = virtqueue_get_buffer_lock(vq, NULL, NULL, &priv->slock);
            if (sem == respsem)
            {
                break;
            }
            else if (sem != NULL)
            {
                TTOS_ReleaseSema(sem);
            }
        }
    }
    else
    {
        ttos_ret = TTOS_ObtainSemaUninterruptable(respsem, TTOS_WAIT_FOREVER);
    }
    return ttos_ret;
}
/****************************************************************************
 * Name: virtio_blk_rdwr
 *
 * Description:
 *   Common function for read and write
 *
 ****************************************************************************/

static ssize_t virtio_blk_rdwr(struct virtio_blk_priv_s *priv, void *buffer, blkcnt_t startsector,
                               unsigned int nsectors, bool write)
{
    struct virtio_device *vdev = priv->vdev;
    struct virtqueue *vq = vdev->vrings_info[0].vq;
    struct virtqueue_buf vb[3];
    SEMA_ID respsem;
    ssize_t ret;
    int readnum;
    irq_flags_t flags;
    T_TTOS_ReturnCode ttos_ret;

    if (nsectors == 0)
    {
        return 0;
    }

    if (ttosIsISR() || ttosIsIdleTask(ttosGetRunningTask()))
    {
        virtqueue_disable_cb_lock(vq, &priv->slock);
    }

    ttos_ret = TTOS_CreateSemaEx(0, &respsem);
    if (ttos_ret != TTOS_OK)
    {
        printk("%s %d create sem failed\n", __func__, __LINE__);
        ret = -EIO;
        goto err;
    }
    /* Build the block request */

    priv->req->type = write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
    priv->req->reserved = 0;
    priv->req->sector = startsector;
    priv->resp->status = 0xff;

    /* Fill the virtqueue buffer:
     * Buffer 0: the block out header;
     * Buffer 1: the read/write buffer;
     * Buffer 2: the block in header, return the status.
     */

    vb[0].buf = priv->req;
    vb[0].len = VIRTIO_BLK_REQ_HEADER_SIZE;
    vb[1].buf = buffer;
    vb[1].len = nsectors * VIRTIO_BLK_SECTOR_SIZE;
    vb[2].buf = priv->resp;
    vb[2].len = VIRTIO_BLK_RESP_HEADER_SIZE;
    readnum = write ? 2 : 1;

    spin_lock_irqsave(&priv->slock, flags);
    ret = virtqueue_add_buffer(vq, vb, readnum, 3 - readnum, respsem);
    if (ret < 0)
    {
        spin_unlock_irqrestore(&priv->slock, flags);
        KLOG_E("virtqueue_add_buffer failed, ret=%" PRIdPTR, ret);
        goto err;
    }

    virtqueue_kick(vq);
    spin_unlock_irqrestore(&priv->slock, flags);

    /* Wait for the request completion */
    ttos_ret = virtio_blk_wait_complete(vq, respsem);
    if (ttos_ret != TTOS_OK)
    {
        ret = -ttos_ret_to_errno(ttos_ret);
        goto err;
    }

    if (priv->resp->status != VIRTIO_BLK_S_OK)
    {
        KLOG_E("%s Error", write ? "Write" : "Read");
        ret = -EIO;
    }
err:
    ttos_ret = TTOS_DeleteSema(respsem);
    if (ttos_ret != TTOS_OK)
    {
        KLOG_E("%s %d delete sem failed\n", __func__, __LINE__);
    }
    if (ttosIsISR() || ttosIsIdleTask(ttosGetRunningTask()))
    {
        virtqueue_enable_cb_lock(vq, &priv->slock);
    }
    return ret >= 0 ? nsectors : ret;
}

/****************************************************************************
 * Name: virtio_blk_open
 *
 * Description: Open the block device
 *
 ****************************************************************************/

static int virtio_blk_open(struct inode *inode)
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

static int virtio_blk_close(struct inode *inode)
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

static ssize_t virtio_blk_read(struct inode *inode, unsigned char *buffer, blkcnt_t startsector,
                               unsigned int nsectors)
{
    struct virtio_blk_priv_s *priv;

    assert(inode->i_private);
    priv = inode->i_private;
    return virtio_blk_rdwr(priv, buffer, startsector, nsectors, false);
}

/****************************************************************************
 * Name: virtio_blk_write
 *
 * Description:
 *   Write the specified number of sectors to the write buffer or to the
 *   physical device.
 *
 ****************************************************************************/

static ssize_t virtio_blk_write(struct inode *inode, const unsigned char *buffer,
                                blkcnt_t startsector, unsigned int nsectors)
{
    struct virtio_blk_priv_s *priv;

    assert(inode->i_private);
    priv = inode->i_private;
    return virtio_blk_rdwr(priv, (void *)buffer, startsector, nsectors, true);
}

/****************************************************************************
 * Name: virtio_blk_geometry
 *
 * Description: Return device geometry
 *
 ****************************************************************************/

static int virtio_blk_geometry(struct inode *inode, struct geometry *geometry)
{
    struct virtio_blk_priv_s *priv;
    int ret = -EINVAL;

    assert(inode->i_private);
    priv = inode->i_private;

    if (geometry)
    {
        geometry->geo_available = true;
        geometry->geo_mediachanged = false;
        geometry->geo_writeenabled = true;
        geometry->geo_nsectors = priv->nsectors;
        geometry->geo_sectorsize = VIRTIO_BLK_SECTOR_SIZE;
        ret = 0;
    }

    return ret;
}

/****************************************************************************
 * Name: virtio_blk_ioctl
 ****************************************************************************/

static int virtio_blk_flush(struct virtio_blk_priv_s *priv)
{
    struct virtio_device *vdev = priv->vdev;
    struct virtqueue *vq = vdev->vrings_info[0].vq;
    struct virtqueue_buf vb[2];
    SEMA_ID respsem;
    int ret;
    irq_flags_t flags;
    T_TTOS_ReturnCode ttos_ret;
    ttos_ret = TTOS_CreateSemaEx(0, &respsem);
    if (ttos_ret != TTOS_OK)
    {
        printk("%s %d create sem failed\n", __func__, __LINE__);
        ret = -EIO;
        return ret;
    }

    /* Build the block request */

    priv->req->type = VIRTIO_BLK_T_FLUSH;
    priv->req->reserved = 0;
    priv->req->sector = 0;
    priv->resp->status = VIRTIO_BLK_S_IOERR;

    vb[0].buf = priv->req;
    vb[0].len = VIRTIO_BLK_REQ_HEADER_SIZE;
    vb[1].buf = priv->resp;
    vb[1].len = VIRTIO_BLK_RESP_HEADER_SIZE;
    spin_lock_irqsave(&priv->slock, flags);
    ret = virtqueue_add_buffer(vq, vb, 1, 1, respsem);
    if (ret < 0)
    {
        spin_unlock_irqrestore(&priv->slock, flags);
        goto err;
    }

    virtqueue_kick(vq);
    spin_unlock_irqrestore(&priv->slock, flags);

    /* Wait for the request completion */

    ret = TTOS_ObtainSemaUninterruptable(respsem, TTOS_WAIT_FOREVER);

    if (priv->resp->status != VIRTIO_BLK_S_OK)
    {
        KLOG_E("Flush Error");
        if (ret != TTOS_OK)
        {
            ret = -ttos_ret_to_errno(ret);
        }
        else
        {
            ret = -EIO;
        }
    }
err:
    ttos_ret = TTOS_DeleteSema(respsem);
    if (ttos_ret != TTOS_OK)
    {
        KLOG_E("%s %d delete sem failed\n", __func__, __LINE__);
    }
    return ret;
}

/****************************************************************************
 * Name: virtio_blk_ioctl
 ****************************************************************************/

static int virtio_blk_ioctl(struct inode *inode, int cmd, unsigned long arg)
{
    struct virtio_blk_priv_s *priv;
    int ret = -ENOTTY;

    assert(inode->i_private);
    priv = inode->i_private;

    switch (cmd)
    {
    case BIOC_FLUSH:
        if (virtio_has_feature(priv->vdev, VIRTIO_BLK_F_FLUSH))
            ret = virtio_blk_flush(priv);
        break;
    }

    return ret;
}

/****************************************************************************
 * Name: virtio_blk_done
 ****************************************************************************/

static void virtio_blk_done(struct virtqueue *vq)
{
    SEMA_ID respsem;
    while (1)
    {
        respsem = virtqueue_get_buffer(vq, NULL, NULL);
        if (respsem != NULL)
        {
            TTOS_ReleaseSema(respsem);
        }
        else
        {
            break;
        }
    }
}

/****************************************************************************
 * Name: virtio_blk_init
 ****************************************************************************/

static int virtio_blk_init(struct virtio_blk_priv_s *priv, struct virtio_device *vdev)
{
    const char *vqname[1];
    vq_callback callback[1];
    int ret;

    priv->vdev = vdev;
    vdev->priv = priv;
    INIT_SPIN_LOCK(&priv->slock);

    /* Alloc the request and in header from tansport layer */

    priv->req = virtio_alloc_buf(vdev, sizeof(*priv->req), 16);
    if (priv->req == NULL)
    {
        ret = -ENOMEM;
        return ret;
    }

    priv->resp = virtio_alloc_buf(vdev, sizeof(*priv->resp), 16);
    if (priv->resp == NULL)
    {
        ret = -ENOMEM;
        goto err_with_req;
    }

    /* Initialize the virtio device */

    virtio_set_status(vdev, VIRTIO_CONFIG_STATUS_DRIVER);
    virtio_set_features(vdev, 0);
    virtio_set_status(vdev, VIRTIO_CONFIG_FEATURES_OK);

    vqname[0] = "virtio_blk_vq";
    callback[0] = virtio_blk_done;
    ret = virtio_create_virtqueues(vdev, 0, 1, vqname, callback);
    if (ret < 0)
    {
        KLOG_E("virtio_device_create_virtqueue failed, ret=%d", ret);
        goto err_with_resp;
    }

    virtio_set_status(vdev, VIRTIO_CONFIG_STATUS_DRIVER_OK);
    virtqueue_enable_cb(vdev->vrings_info[0].vq);
    return 0;

err_with_resp:
    virtio_free_buf(vdev, priv->resp);
err_with_req:
    virtio_free_buf(vdev, priv->req);
    return ret;
}

/****************************************************************************
 * Name: virtio_blk_uninit
 ****************************************************************************/

static void virtio_blk_uninit(struct virtio_blk_priv_s *priv)
{
    struct virtio_device *vdev = priv->vdev;

    virtio_reset_device(vdev);
    virtio_delete_virtqueues(vdev);
    virtio_free_buf(vdev, priv->resp);
    virtio_free_buf(vdev, priv->req);
}

/****************************************************************************
 * Name: virtio_blk_probe
 ****************************************************************************/

static int virtio_blk_probe(struct virtio_device *vdev)
{
    struct virtio_blk_priv_s *priv;
    int ret;
    struct device *dev = &vdev->device;

    /* 使用托管内存分配 */
    priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
    if (priv == NULL)
    {
        KLOG_E("No enough memory");
        return -ENOMEM;
    }

    /* Init the virtio block driver */
    ret = virtio_blk_init(priv, vdev);
    if (ret < 0)
    {
        KLOG_E("virtio_blk_init failed, ret=%d", ret);
        return ret;
    }

    /* Read the block config and save the capacity to nsectors */
    virtio_read_config_member(priv->vdev, struct virtio_blk_config_s, capacity, &priv->nsectors);
    KLOG_I("Virio blk capacity=%" PRIu64 " sectors", priv->nsectors);

    INIT_SPIN_LOCK(&priv->slock);

    /* Register block driver */
    snprintf(priv->name, NAME_MAX, "/dev/virtblk%d", g_virtio_blk_idx);
    ret = register_blockdriver(priv->name, &g_virtio_blk_bops, 0660, priv);
    if (ret < 0)
    {
        KLOG_E("Register block driver failed, ret=%d", ret);
        virtio_blk_uninit(priv);
        return ret;
    }
    g_virtio_blk_idx++;
    return ret;
}

/****************************************************************************
 * Name: virtio_blk_remove
 ****************************************************************************/

static void virtio_blk_remove(struct virtio_device *vdev)
{
    struct virtio_blk_priv_s *priv = vdev->priv;

    /* 注销块设备驱动 */
    unregister_driver(priv->name);
    virtio_blk_uninit(priv);

    /* 所有资源（内存）由 devres 自动释放 */
    return;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: virtio_register_blk_driver
 ****************************************************************************/

static int virtio_register_blk_driver(void)
{
    return virtio_add_driver(&g_virtio_blk_driver);
}
INIT_EXPORT_DRIVER(virtio_register_blk_driver, "virtio_register_blk_driver");
