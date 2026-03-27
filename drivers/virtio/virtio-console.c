/****************************************************************************
 * drivers/virtio/virtio-console.c
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

#include "virtio.h"
#include <circbuf.h>
#include <driver/class/chardev.h>
#include <driver/devbus.h>
#include <driver/device.h>
#include <driver/driver.h>
#include <driver/serial/serial_core.h>
#include <kmalloc.h>
#include <ttosProcess.h>
#include <ttos_init.h>
#include <tty.h>

#define KLOG_TAG "VIRTIO_CONSOLE"
#include <klog.h>

#define VIRTIO_SERIAL_RX 0
#define VIRTIO_SERIAL_TX 1
#define VIRTIO_SERIAL_NUM 2

struct xfer_buffer
{
    //   mutex_t              lock;   /* Used to control exclusive access
    //                                 * to the buffer */
    volatile size_t head; /* Index to the head [IN] index
                           * in the buffer */
    volatile size_t tail; /* Index to the tail [OUT] index
                           * in the buffer */
    size_t size;          /* The allocated size of the buffer */
    char *buffer;         /* Pointer to the allocated buffer memory */
};

struct virtio_serial_priv_s
{
    /* Virtio device information */

    struct virtio_device *vdev;

    struct xfer_buffer xmit;
    struct xfer_buffer recv;

    struct device *udev;
    char name[NAME_MAX];
    spinlock_t lock;
};

void char_xmitchars_done(struct device *dev)
{
    struct uart_dmaxfer_s *xfer = &((struct char_class_type *)dev->class)->dmatx;
    size_t nbytes = xfer->nbytes;
    struct char_class_type *class = (struct char_class_type *)dev->class;

    if (nbytes)
    {
        circbuf_readcommit(&class->xmit.buf, nbytes);
    }

    /* Reset xmit buffer. */

    xfer->nbytes = 0;
    xfer->length = xfer->nlength = 0;

    /* If any bytes were removed from the buffer, inform any waiters there
     * there is space available.
     */

    if (nbytes)
    {
        char_datasent(dev);
    }
}

static void char_sync_buff_to_dev(struct device *dev)
{
    struct uart_dmaxfer_s *xfer = &((struct char_class_type *)dev->class)->dmatx;
    struct xfer_buffer *rxbuf = &((struct virtio_serial_priv_s *)dev->priv)->recv;
    struct char_class_type *class = (struct char_class_type *)dev->class;
    int tail;
    tail = rxbuf->tail;
    do
    {
        /* Check if the TX buffer is full */

        if (rxbuf->head != tail)
        {
            /* No.. not full.  Add the character to the TX buffer and return. */
            if (circbuf_write(&class->recv.buf, &rxbuf->buffer[tail], 1))
            {
                if (++tail >= rxbuf->size)
                {
                    tail = 0;
                }

                rxbuf->tail = tail;
            }
            else
            {
                break;
            }
        }
    } while (rxbuf->head != tail);
}

static void char_prepare_xfer(struct device *dev)
{
    struct char_class_type *class = (struct char_class_type *)dev->class;
    struct uart_dmaxfer_s *xfer = &class->dmatx;
    size_t used;
    size_t len1;
    size_t len2;
    char *buf1;

    used = circbuf_used(&class->xmit.buf);
    if (used == 0)
    {
        return;
    }

    buf1 = circbuf_get_readptr(&class->xmit.buf, &len1);
    if (len1 > used)
    {
        len1 = used;
    }

    len2 = used - len1;

    xfer->buffer = buf1;
    xfer->length = len1;

    if (len2)
    {
        xfer->nbuffer = class->xmit.buf.base;
        xfer->nlength = len2;
    }
    else
    {
        xfer->nbuffer = NULL;
        xfer->nlength = 0;
    }
}

void char_xmitchars_dma(struct device *dev)
{
    struct uart_dmaxfer_s *xfer = &((struct char_class_type *)dev->class)->dmatx;
    if (xfer->length != 0)
    {
        return;
    }

    if (circbuf_is_empty(&((struct char_class_type *)dev->class)->xmit.buf))
    {
        /* No data to transfer. */

        return;
    }

    char_prepare_xfer(dev);

    char_dmasend(dev);
}

void char_recvchars_dma(struct device *dev)
{
    struct uart_dmaxfer_s *xfer = &((struct char_class_type *)dev->class)->dmarx;
    struct xfer_buffer *rxbuf = &((struct virtio_serial_priv_s *)dev->priv)->recv;

    bool is_full;
    int nexthead;

    /* Get the next head index and check if there is room to adding another
     * byte to the buffer.
     */

    nexthead = rxbuf->head + 1;
    if (nexthead >= rxbuf->size)
    {
        nexthead = 0;
    }

    is_full = nexthead == rxbuf->tail;

    if (is_full)
    {
        /* If there is no free space in receive buffer we cannot start DMA
         * transfer.
         */

        return;
    }

    if (rxbuf->tail <= rxbuf->head)
    {
        xfer->buffer = &rxbuf->buffer[rxbuf->head];
        xfer->nbuffer = rxbuf->buffer;

        if (rxbuf->tail > 0)
        {
            xfer->length = rxbuf->size - rxbuf->head;
            xfer->nlength = rxbuf->tail - 1;
        }
        else
        {
            xfer->length = rxbuf->size - rxbuf->head - 1;
            xfer->nlength = 0;
        }
    }
    else
    {
        xfer->buffer = &rxbuf->buffer[rxbuf->head];
        xfer->length = rxbuf->tail - rxbuf->head - 1;
        xfer->nbuffer = NULL;
        xfer->nlength = 0;
    }

    char_dmareceive(dev);
}

void char_recvchars_done(struct device *dev)
{
    struct uart_dmaxfer_s *xfer = &((struct char_class_type *)dev->class)->dmarx;
    struct xfer_buffer *rxbuf = &((struct virtio_serial_priv_s *)dev->priv)->recv;
    struct char_class_type *class = (struct char_class_type *)dev->class;
    size_t nbytes = xfer->nbytes;
#if defined(CONFIG_TTY_SIGINT) || defined(CONFIG_TTY_SIGTSTP) ||                                   \
    defined(CONFIG_TTY_FORCE_PANIC) || defined(CONFIG_TTY_LAUNCH)
    int signo = 0;

    /* Check if the SIGINT character is anywhere in the newly received DMA
     * buffer.
     */

    signo = tty_check_special(class->tc_lflag, &rxbuf->buffer[rxbuf->head], nbytes);
#endif

    /* Move head for nbytes. */

    rxbuf->head = (rxbuf->head + nbytes) % rxbuf->size;
    xfer->nbytes = 0;
    xfer->length = xfer->nlength = 0;

    /* If any bytes were added to the buffer, inform any waiters there is new
     * incoming data available.
     */

    if (rxbuf->head >= rxbuf->tail)
    {
        nbytes = rxbuf->head - rxbuf->tail;
    }
    else
    {
        nbytes = rxbuf->size - rxbuf->tail + rxbuf->head;
    }

#ifdef CONFIG_SERIAL_TERMIOS
    if (nbytes >= dev->minrecv)
#else
    if (nbytes)
#endif
    {
        char_sync_buff_to_dev(dev);
        char_datareceived(dev);
    }

#if defined(CONFIG_TTY_SIGINT) || defined(CONFIG_TTY_SIGTSTP) ||                                   \
    defined(CONFIG_TTY_FORCE_PANIC) || defined(CONFIG_TTY_LAUNCH)
    /* Send the signal if necessary */

    if (signo != 0)
    {
        if (class->gpid != INVALID_PROCESS_ID)
        {
            kernel_signal_send(class->gpid, TO_PGROUP, signo, SI_KERNEL, 0);
        }
    }
#endif
}

/* Uart operation functions */

static int virtio_serial_setup(struct device *dev);
static void virtio_serial_shutdown(struct device *dev);
static int virtio_serial_attach(struct device *dev);
static void virtio_serial_detach(struct device *dev);
static int virtio_serial_ioctl(struct file *filep, int cmd, unsigned long arg);
static void virtio_serial_rxint(struct device *dev, bool enable);
static void virtio_serial_send(struct device *dev, int ch);
static void virtio_serial_txint(struct device *dev, bool enable);
static bool virtio_serial_txready(struct device *dev);
static bool virtio_serial_txempty(struct device *dev);
static void virtio_serial_dmasend(struct device *dev);
static void virtio_serial_dmatxavail(struct device *dev);
static void virtio_serial_dmareceive(struct device *dev);
static void virtio_serial_dmarxfree(struct device *dev);

/* Other functions */

static void virtio_serial_rxready(struct virtqueue *vq);
static void virtio_serial_txdone(struct virtqueue *vq);

static int virtio_serial_probe(struct virtio_device *vdev);
static void virtio_serial_remove(struct virtio_device *vdev);

static const struct char_ops_s g_virtio_serial_ops = {
    .setup = virtio_serial_setup,       /* setup */
    .shutdown = virtio_serial_shutdown, /* shutdown */
    .attach = virtio_serial_attach,     /* attach */
    .detach = virtio_serial_detach,     /* detach */
    .ioctl = virtio_serial_ioctl,       /* ioctl */
    .receive = NULL,                    /* receive */
    .rxint = virtio_serial_rxint,       /* rxint */
    .rxavailable = NULL,                /* rxavailable */
#ifdef CONFIG_SERIAL_IFLOWCONTROL
    .rxflowcontrol = NULL, /* rxflowcontrol */
#endif
    .dmasend = virtio_serial_dmasend,       /* dmasend */
    .dmareceive = virtio_serial_dmareceive, /* dmareceive */
    .dmarxfree = virtio_serial_dmarxfree,   /* dmarxfree */
    .dmatxavail = virtio_serial_dmatxavail, /* dmatxavail */
    .send = virtio_serial_send,             /* send */
    .txint = virtio_serial_txint,           /* txint */
    .txready = virtio_serial_txready,       /* txready */
    .txempty = virtio_serial_txempty,       /* txempty */
};

static int g_virtio_serial_idx = 0;

static int virtio_serial_setup(struct device *dev)
{
    return 0;
}

static void virtio_serial_shutdown(struct device *dev)
{
    /* Nothing */
}

static int virtio_serial_attach(struct device *dev)
{
    struct virtio_serial_priv_s *priv = dev->priv;
    struct virtqueue *rx_vq = priv->vdev->vrings_info[VIRTIO_SERIAL_RX].vq;
    struct virtqueue *tx_vq = priv->vdev->vrings_info[VIRTIO_SERIAL_TX].vq;

    virtqueue_enable_cb(rx_vq);
    virtqueue_enable_cb(tx_vq);
    return 0;
}

static void virtio_serial_detach(struct device *dev)
{
    struct virtio_serial_priv_s *priv = dev->priv;
    struct virtqueue *rx_vq = priv->vdev->vrings_info[VIRTIO_SERIAL_RX].vq;
    struct virtqueue *tx_vq = priv->vdev->vrings_info[VIRTIO_SERIAL_TX].vq;

    virtqueue_disable_cb(rx_vq);
    virtqueue_disable_cb(tx_vq);
}

static int virtio_serial_ioctl(struct file *filep, int cmd, unsigned long arg)
{
    return -ENOTTY;
}

static void virtio_serial_rxint(struct device *dev, bool enable) {}

static void virtio_serial_send(struct device *dev, int ch)
{
    struct char_class_type *class = (struct char_class_type *)dev->class;

    if (!circbuf_is_full(&class->xmit.buf))
    {
        /* No.. not full.  Add the character to the TX buffer and return. */

        circbuf_write(&class->xmit.buf, &ch, 1);
    }

    char_dmatxavail(dev);
}

static void virtio_serial_txint(struct device *dev, bool enable) {}
static bool virtio_serial_txready(struct device *dev)
{
    struct char_class_type *class = (struct char_class_type *)dev->class;

    return !circbuf_is_full(&class->xmit.buf);
}

static bool virtio_serial_txempty(struct device *dev)
{
    return true;
}

static void virtio_serial_dmasend(struct device *dev)
{
    struct virtio_serial_priv_s *priv = dev->priv;
    struct char_class_type *class = (struct char_class_type *)dev->class;
    struct virtqueue *vq = priv->vdev->vrings_info[VIRTIO_SERIAL_TX].vq;
    struct uart_dmaxfer_s *xfer = &class->dmatx;
    struct virtqueue_buf vb[2];
    irq_flags_t flags;
    uintptr_t len;
    int ret;
    int num = 1;

    /* Get the total send length */

    len = xfer->length + xfer->nlength;

    /* Set the virtqueue buffer */

    vb[0].buf = xfer->buffer;
    vb[0].len = xfer->length;

    if (xfer->nlength != 0)
    {
        vb[1].buf = xfer->nbuffer;
        vb[1].len = xfer->nlength;
        num = 2;
    }

    /* Add buffer to TX virtiqueue and notify the other size */

    spin_lock_irqsave(&priv->lock, flags);
    ret = virtqueue_add_buffer(vq, vb, num, 0, (void *)len);
    if (ret == 0)
    {
        virtqueue_kick(vq);
    }
    spin_unlock_irqrestore(&priv->lock, flags);

    if (ret != 0)
    {
        /* Queue is full, keep data in circbuf and retry later. */
        xfer->length = 0;
        xfer->nlength = 0;
    }
}

static void virtio_serial_dmatxavail(struct device *dev)
{
    struct char_class_type *class = (struct char_class_type *)dev->class;
    if (class->dmatx.length == 0)
    {
        char_xmitchars_dma(dev);
    }
}

static void virtio_serial_dmareceive(struct device *dev)
{
    struct virtio_serial_priv_s *priv = dev->priv;
    struct char_class_type *class = (struct char_class_type *)dev->class;
    struct virtqueue *vq = priv->vdev->vrings_info[VIRTIO_SERIAL_RX].vq;
    struct uart_dmaxfer_s *xfer = &class->dmarx;
    struct virtqueue_buf vb[2];
    irq_flags_t flags;
    int num = 1;

    vb[0].buf = xfer->buffer;
    vb[0].len = xfer->length;

    if (xfer->nlength != 0)
    {
        vb[num].buf = xfer->nbuffer;
        vb[num].len = xfer->nlength;
        num = 2;
    }

    /* Add buffer to the RX virtqueue and notify the device side */

    spin_lock_irqsave(&priv->lock, flags);
    virtqueue_add_buffer(vq, vb, 0, num, xfer);
    virtqueue_kick(vq);
    spin_unlock_irqrestore(&priv->lock, flags);
}

static void virtio_serial_dmarxfree(struct device *dev)
{
    struct char_class_type *class = (struct char_class_type *)dev->class;
    if (class->dmarx.length == 0)
    {
        char_recvchars_dma(dev);
    }
}

static void virtio_serial_rxready(struct virtqueue *vq)
{
    struct virtio_serial_priv_s *priv = vq->vq_dev->priv;
    struct uart_dmaxfer_s *xfer;
    uint32_t len;

    /* Received some data, call uart_recvchars_done() */

    xfer = virtqueue_get_buffer_lock(vq, &len, NULL, &priv->lock);
    if (xfer == NULL)
    {
        return;
    }

    xfer->nbytes = len;
    char_recvchars_done(priv->udev);
    char_dmarxfree(priv->udev);
}

static void virtio_serial_txdone(struct virtqueue *vq)
{
    struct virtio_serial_priv_s *priv = vq->vq_dev->priv;
    struct char_class_type *class = (struct char_class_type *)priv->udev->class;
    uintptr_t len;

    /* Call uart_xmitchars_done to notify the upperhalf */

    len = (uintptr_t)virtqueue_get_buffer_lock(vq, NULL, NULL, &priv->lock);

    class->dmatx.nbytes = len;
    char_xmitchars_done(priv->udev);
    char_dmatxavail(priv->udev);
}

static int virtio_serial_init(struct virtio_serial_priv_s *priv, struct virtio_device *vdev)
{
    const char *vqnames[VIRTIO_SERIAL_NUM];
    vq_callback callbacks[VIRTIO_SERIAL_NUM];
    struct device *udev;
    int ret;

    priv->vdev = vdev;
    vdev->priv = priv;
    spin_lock_init(&priv->lock);

    /* Uart device buffer and ops init */
    priv->udev = &vdev->device;
    udev = priv->udev;
    udev->priv = priv;
    priv->recv.size = CONFIG_UART_RX_BUF_SIZE;
    priv->recv.buffer = virtio_zalloc_buf(vdev, priv->recv.size, 16);
    if (priv->recv.buffer == NULL)
    {
        KLOG_E("No enough memory");
        return -ENOMEM;
    }

    priv->xmit.size = CONFIG_UART_TX_BUF_SIZE;
    priv->xmit.buffer = virtio_zalloc_buf(vdev, priv->xmit.size, 16);
    if (priv->xmit.buffer == NULL)
    {
        KLOG_E("No enough memory");
        ret = -ENOMEM;
        goto err_with_recv;
    }

    /* Initialize the virtio device */

    virtio_set_status(vdev, VIRTIO_CONFIG_STATUS_DRIVER);
    virtio_set_features(vdev, 0);
    virtio_set_status(vdev, VIRTIO_CONFIG_FEATURES_OK);

    vqnames[VIRTIO_SERIAL_RX] = "virtio_serial_rx";
    vqnames[VIRTIO_SERIAL_TX] = "virtio_serial_tx";
    callbacks[VIRTIO_SERIAL_RX] = virtio_serial_rxready;
    callbacks[VIRTIO_SERIAL_TX] = virtio_serial_txdone;
    ret = virtio_create_virtqueues(vdev, 0, VIRTIO_SERIAL_NUM, vqnames, callbacks);
    if (ret < 0)
    {
        KLOG_E("virtio_device_create_virtqueue failed, ret=%d", ret);
        goto err_with_xmit;
    }

    virtio_set_status(vdev, VIRTIO_CONFIG_STATUS_DRIVER_OK);
    return 0;

err_with_xmit:
    virtio_free_buf(vdev, priv->xmit.buffer);
err_with_recv:
    virtio_free_buf(vdev, priv->recv.buffer);
    virtio_reset_device(vdev);
    return ret;
}

static void virtio_serial_uninit(struct virtio_serial_priv_s *priv)
{
    struct virtio_device *vdev = priv->vdev;

    virtio_reset_device(vdev);
    virtio_delete_virtqueues(vdev);
    virtio_free_buf(vdev, priv->xmit.buffer);
    virtio_free_buf(vdev, priv->recv.buffer);
}

static int virtio_serial_uart_register(struct virtio_serial_priv_s *priv)
{
    int ret;
    struct device *dev = &priv->vdev->device;
    struct char_class_type *class;
    void *rx_buf, *tx_buf;

    /* 使用托管内存分配 char_class */
    class = devm_kzalloc(dev, sizeof(struct char_class_type), GFP_KERNEL);
    if (!class)
        return -ENOMEM;

    dev->class = (struct devclass_type *)class;

    /* 分配接收和发送缓冲区 */
    rx_buf = devm_kmalloc(dev, CONFIG_UART_RX_BUF_SIZE, GFP_KERNEL);
    if (!rx_buf)
        return -ENOMEM;

    tx_buf = priv->xmit.buffer;

    circbuf_init(&class->recv.buf, rx_buf, CONFIG_UART_RX_BUF_SIZE);
    circbuf_init(&class->xmit.buf, tx_buf, CONFIG_UART_TX_BUF_SIZE);

    /* 初始化锁和信号量 */
    spin_lock_init(&class->xmit_spinlock);
    spin_lock_init(&class->recv_spinlock);

    /* 设置默认窗口大小 */
    class->winsize.ws_col = 80;
    class->winsize.ws_row = 24;

    class->class.device = dev;
    spin_lock_init(&class->class.lock);

    /* 使用通用的 UART 操作函数 */
    class->ops = &g_virtio_serial_ops;
    class->isconsole = true;

    ret = serial_register(dev);

    return ret;
}

static int virtio_serial_probe(struct virtio_device *vdev)
{
    struct virtio_serial_priv_s *priv;
    int ret;

    /* Alloc the virtio serial driver and uart buffer */

    priv = kzalloc(sizeof(*priv), GFP_KERNEL);
    if (priv == NULL)
    {
        KLOG_E("No enough memory");
        return -ENOMEM;
    }

    ret = virtio_serial_init(priv, vdev);
    if (ret < 0)
    {
        KLOG_E("virtio_serial_init failed, ret=%d", ret);
        goto err_with_priv;
    }

    /* Uart driver register */

    ret = virtio_serial_uart_register(priv);
    if (ret < 0)
    {
        KLOG_E("uart_register failed, ret=%d", ret);
        goto err_with_init;
    }

    return ret;

err_with_init:
    virtio_serial_uninit(priv);
err_with_priv:
    kfree(priv);
    return ret;
}

static void virtio_serial_remove(struct virtio_device *vdev)
{
    struct virtio_serial_priv_s *priv = vdev->priv;

    virtio_serial_uninit(priv);
    kfree(priv);
}

static struct virtio_driver g_virtio_console_driver = {
    .device = VIRTIO_ID_CONSOLE, /* device id */
    .driver.name = "virtconsole",
    .probe = virtio_serial_probe,   /* probe */
    .remove = virtio_serial_remove, /* remove */
};

static struct virtio_driver g_virtio_rprocserial_driver = {
    .device = VIRTIO_ID_RPROC_SERIAL, /* device id */
    .driver.name = "virtconsole",
    .probe = virtio_serial_probe,   /* probe */
    .remove = virtio_serial_remove, /* remove */
};

static int virtio_register_console_driver(void)
{
    int ret1 = virtio_add_driver(&g_virtio_console_driver);
    int ret2 = virtio_add_driver(&g_virtio_rprocserial_driver);
    return ret1 < 0 ? ret1 : ret2;
}
INIT_EXPORT_DRIVER(virtio_register_console_driver, "virtio_register_console_driver");
