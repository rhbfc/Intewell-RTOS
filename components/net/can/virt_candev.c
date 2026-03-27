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

#include <version.h>
#include <barrier.h>
#include <driver/devbus.h>
#include <driver/device.h>
#include <driver/driver.h>
#include <driver/of.h>
#if INTEWELL_VERSION_MINOR>=1
#include <driver/platform.h>
#endif
#include <errno.h>
#include <fs/fs.h>
#include <fs/ioctl.h>
#include <inttypes.h>
#include <io.h>
#include <stdio.h>
#include <ttosMM.h>
#include <ttos_init.h>
#include <ttos_pic.h>
#include <uaccess.h>
#include <unistd.h>
#include <wqueue.h>
#include <net/can_dev.h>

#undef KLOG_TAG
#define KLOG_TAG "VirtCAN"
#include <klog.h>


/* The virtcan_driver_s encapsulates all state information for a single
 * hardware interface
 */

struct virtcan_driver_s
{
    uint8_t name[32];
    devaddr_region_t region[1]; /* virtcan address info */
    uintptr_t base;             /* register base address (VA) */
    uint32_t irq;               /* irq number */
    uint32_t clk_freq;          /* Peripheral clock frequency */
    bool loopback;              /* loopback true:enable false:disable */
    bool selfrecept;            /* Self-Reception true:enable false:disable */
    bool canfd_capable;

    struct work_s rcvwork;  /* For deferring interrupt work to the wq */
    struct work_s irqwork;  /* For deferring interrupt work to the wq */
    struct work_s pollwork; /* For deferring poll work to the work wq */
    struct work_s errwork;

    size_t packetloss; /* Packet loss number */
};

/****************************************************************************
 * Private Data
 ****************************************************************************/
extern CAN_DEV_INFO virtcan_dev_info;

/* callback functions */
static int virtcan_ifup(struct file *filep);
static int virtcan_ifdown(struct file *filep);
static int virtcan_ioctl_handler(struct virtcan_driver_s *dev, int cmd, unsigned long arg);

/* Initialization */
static int virtcan_initialize(struct virtcan_driver_s *priv);
static void virtcan_reset(struct virtcan_driver_s *priv);

/****************************************************************************
 * Function: virtcan_ifup
 *
 * Description:
 *   Bring up the CAN interface
 *
 * Input Parameters:
 *   dev  - Reference to the driver state structure
 *
 * Returned Value:
 *   0 or ERROR
 *
 * Assumptions:
 *
 ****************************************************************************/

static int virtcan_ifup(struct file *filep)
{
    struct virtcan_driver_s *priv = filep->f_inode->i_private;

    return 0;
}

/****************************************************************************
 * Function: virtcan_ifdown
 *
 * Description:
 *   Stop the CAN interface.
 *
 * Input Parameters:
 *   dev  - Reference to the driver state structure
 *
 * Returned Value:
 *   None
 *
 * Assumptions:
 *
 ****************************************************************************/

static int virtcan_ifdown(struct file *filep)
{
    struct virtcan_driver_s *priv = filep->f_inode->i_private;

    return 0;
}

static int virtcan_ioctl_handler(struct virtcan_driver_s *priv, int cmd, unsigned long arg)
{
    int ret;
    unsigned int request;
    uint32_t objsize = 0;

    request = (unsigned int)cmd;

    switch (request)
    {
        default:
            ret = -EPERM;
            break;
    }

    return ret;
}

static int virtcan_initialize(struct virtcan_driver_s *priv)
{
    uint32_t tdcoff;
    int i;
    volatile struct mb_s *mb;

    return 0;
}

static void virtcan_reset(struct virtcan_driver_s *priv)
{
    return;
}

static int virtcan_open(struct file *filep)
{
    int ret;
    struct virtcan_driver_s *priv;

    if (filep == NULL)
    {
        return -EINVAL;
    }

    printk("virtcan_open\n");

    priv = filep->f_inode->i_private;

    return 0;
}

static int virtcan_close(struct file *filep)
{
    struct virtcan_driver_s *priv = filep->f_inode->i_private;

    printk("virtcan_close\n");

    return 0;
}

static ssize_t virtcan_read(struct file *filep, char *buffer, size_t buflen)
{
    ssize_t nbytes;
    struct virtcan_driver_s *priv = filep->f_inode->i_private;

    printk("virtcan_read buflen: %d\n", buflen);

    return nbytes;
}

static ssize_t virtcan_write(struct file *filep, const char *buffer, size_t buflen)
{
    CAN_MSG *msg = (CAN_MSG *)buffer;
    struct virtcan_driver_s *priv = filep->f_inode->i_private;
    int i;

    printk("+----------Virt CAN Write----------+\n");
    printk("CAN ID: %x\n", msg->can_id);

    printk("CAN DLC: %d\n", msg->can_dlc);
    printk("CAN Data:");
    for (i = 0; i < msg->can_dlc; i++)
    {
        printk(" %02x", msg->data[i]);
    }
    printk("\n");
    printk("+----------------------------------+\n");

    return msg->can_dlc;
}

static int virtcan_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
    int ret;
    struct virtcan_driver_s *priv = filep->f_inode->i_private;

    if (priv == NULL)
    {
        return (-EINVAL);
    }

    printk("virtcan_ioctl cmd: 0x%08x\n", cmd);

    ret = virtcan_ioctl_handler(priv, cmd, arg);

    return ret;
}

static int virtcan_poll(struct file *filep, struct kpollfd *fds, bool setup)
{
    struct virtcan_driver_s *priv = filep->f_inode->i_private;

    return 0;
}

static const struct file_operations g_virtcan_fops =
{
    .open = virtcan_open,
    .close = virtcan_close,
    .read = virtcan_read,
    .write = virtcan_write,
    .ioctl = virtcan_ioctl,
    .poll = virtcan_poll,
    .mmap = NULL,
};

#if INTEWELL_VERSION_MINOR>=1
int virtcan_probe(struct platform_device *pdev)
#else
int virtcan_probe(struct device *dev)
#endif
{
#if INTEWELL_VERSION_MINOR>=1
    struct device *dev = &pdev->dev;
#endif
    int ret;
    static int index = 0;
    uint32_t txsize;
    uint32_t rxsize;
    size_t regsize;
    phys_addr_t regaddr;
    struct virtcan_driver_s *priv;

    priv = calloc(1, sizeof(struct virtcan_driver_s));
    if (priv == NULL)
    {
        KLOG_E("No enough memory");
        free(priv);
        return -ENOMEM;
    }

#if INTEWELL_VERSION_MINOR>=1
    ret = of_device_get_resource_regs(dev, &priv->region[0], 1);
#else
    ret = platform_get_resource_regs(dev, &priv->region[0], 1);
#endif
    if (ret < 0)
    {
        KLOG_E("device get reg error");
        free(priv);
        return ret;
    }

    snprintf(priv->name, sizeof(dev->name), "/dev/vcan%d", index++);
    ret = register_driver(priv->name, &g_virtcan_fops, 0666, priv);
    if (ret < 0)
    {
        KLOG_E("register driver error");
        free(priv);
        return ret;
    }

    virtcan_dev_info.dev_name = priv->name;

    can_device_init(&virtcan_dev_info, priv, dev);

    return 0;
}

static struct of_device_id virtcan_table[] =
{
    {.compatible = "virt,virtcan"},
    {/* end of list */},
};

#if INTEWELL_VERSION_MINOR>=1
static struct platform_driver virtcan_driver =
{
    .probe = virtcan_probe,
    .driver =
    {
        .name = "virtcan driver",
        .of_match_table = &virtcan_table[0],
    },
};
#else
static struct driver virtcan_driver =
{
    .name = "virtcan driver",
    .probe = virtcan_probe,
    .match_table = &virtcan_table[0]
};
#endif

CAN_DEV_FUNCS virtcan_can_funcs =
{
    .can_ifup = virtcan_ifup,
    .can_ifdown = virtcan_ifdown,
    .can_ioctl = virtcan_ioctl,
    .can_send = virtcan_write,
    .can_recv = virtcan_read,
    .can_poll = virtcan_poll,
    .can_close = NULL,            /* CAN驱动通常无需提供该接口 */
};

CAN_DEV_INFO virtcan_dev_info =
{
    .dev_name = NULL,               /* 调用can_device_init()前需赋值 */
    .can_func = &virtcan_can_funcs,
};

int virtcan_driver_init(void)
{
    platform_add_driver(&virtcan_driver);

    return OK;
}

INIT_EXPORT_DRIVER(virtcan_driver_init, "Virt Can Driver Init");