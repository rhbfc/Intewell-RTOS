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

#include <driver/pci/pci.h>
#include <fs/fs.h>
#include <io.h>
#include <kmalloc.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <ttosMM.h>
#include <uaccess.h>

#define SHMEM_IOCTL_DOORBELL _IOW('i', 1, uint32_t)

#define IVSHMEM_IRQ_MASK 0x0
#define IVSHMEM_IRQ_STATE 0x4
#define IVSHMEM_ID 0x8
#define IVSHMEM_ID_DOORBELL 0xC
#define IVSHMEM_ID_LINK_COUNTS 0x10

#define IVSHMEM_REG_IVSHMEM_NAME0 0x20
#define IVSHMEM_REG_IVSHMEM_NAME1 0x24
#define IVSHMEM_REG_IVSHMEM_NAME2 0x28
#define IVSHMEM_REG_IVSHMEM_NAME3 0x2c

struct ivshmem_dev
{
    void *reg_base;
    void *mem_base;
    phys_addr_t phy_base;
    uint64_t size;
    struct kpollfd *fds;
};

struct ivshmem_data
{
    uint16_t link_id;
    uint16_t vector_id;
};

static ssize_t shmem_read(struct file *filep, char *buffer, size_t len)
{
    struct pci_dev *dev = (struct pci_dev *)filep->f_inode->i_private;
    struct ivshmem_dev *memdev = (struct ivshmem_dev *)dev->priv;

    memcpy(buffer, memdev->mem_base + filep->f_pos, len);
    return len; /* Return EOF */
}

static ssize_t shmem_write(struct file *filep, const char *buffer, size_t len)
{
    struct pci_dev *dev = (struct pci_dev *)filep->f_inode->i_private;
    struct ivshmem_dev *memdev = (struct ivshmem_dev *)dev->priv;

    memcpy(memdev->mem_base + filep->f_pos, buffer, len);
    return len; /* Say that everything was written */
}

static int shmem_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
    struct pci_dev *dev = (struct pci_dev *)filep->f_inode->i_private;
    struct ivshmem_dev *memdev = (struct ivshmem_dev *)dev->priv;
    struct ivshmem_data ivshmem_data;
    uint32_t data;

    if (copy_from_user(&ivshmem_data, (void *)arg, sizeof(struct ivshmem_data)))
    {
        return -EFAULT;
    }

    switch (cmd)
    {
    case SHMEM_IOCTL_DOORBELL:
        data = (ivshmem_data.link_id << 16) | ivshmem_data.vector_id;
        writel(data, memdev->reg_base + IVSHMEM_ID_DOORBELL);
        break;

    default:
        return -EINVAL;
    }

    return 0;
}

static int shmem_poll(struct file *filep, struct kpollfd *fds, bool setup)
{
    struct pci_dev *dev = (struct pci_dev *)filep->f_inode->i_private;
    struct ivshmem_dev *memdev = (struct ivshmem_dev *)dev->priv;

    if (setup)
    {
        memdev->fds = fds;
        if (readl(memdev->reg_base + IVSHMEM_IRQ_STATE))
            kpoll_notify(&fds, 1, POLLIN, NULL);
    }
    else
    {
        memdev->fds = NULL;
    }

    return 0;
}

static int shmem_mmap(struct file *filep, struct mm_region *map)
{
    struct pci_dev *dev = (struct pci_dev *)filep->f_inode->i_private;
    struct ivshmem_dev *memdev = (struct ivshmem_dev *)dev->priv;

    if ((map->region_page_count * PAGE_SIZE) > memdev->size)
        return -EINVAL;

    map->physical_address = ALIGN_DOWN(memdev->phy_base, PAGE_SIZE) + map->offset;
    map->mem_attr = MMAP_SET_ATTR_TYPE(map, MT_MEMORY);
    return 0;
}

static const struct file_operations ivshmem_fops = {
    .read = shmem_read,   /* read */
    .write = shmem_write, /* write */
    .ioctl = shmem_ioctl,
    .poll = shmem_poll,
    .mmap = shmem_mmap,
};

static void ivshmem_handler(uint32_t irq, void *param)
{
    struct ivshmem_dev *memdev = (struct ivshmem_dev *)param;
    // printk("%s: irq: %d  %p \n", __func__, irq, param);

    readl(memdev->reg_base + IVSHMEM_IRQ_STATE);
    if (memdev->fds)
    {
        kpoll_notify(&memdev->fds, 1, POLLIN, NULL);
    }
}

static int ivshmem_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
    struct ivshmem_dev *memdev;
    char ivshmem_name[16];
    int *name = (int *)ivshmem_name;
    static uint32_t num = 0;

    memdev = zalloc(sizeof(*memdev));
    memdev->size = dev->bar[4].size;
    memdev->phy_base = dev->bar[4].addr;
    memdev->reg_base = pci_bar_ioremap(dev, 0);
    memdev->mem_base = pci_bar_ioremap(dev, 4);
    dev->priv = memdev;

    pci_irq_alloc(dev, PCI_INT_LEGACY, 0);
    pci_irq_install(dev->legacy_irq, ivshmem_handler, memdev, "ivshmem");
    pci_irq_umask(dev, dev->legacy_irq);

    *(name) = readl((char *)(memdev->reg_base) + IVSHMEM_REG_IVSHMEM_NAME0);
    *(name + 1) = readl((char *)(memdev->reg_base) + IVSHMEM_REG_IVSHMEM_NAME1);
    *(name + 2) = readl((char *)(memdev->reg_base) + IVSHMEM_REG_IVSHMEM_NAME2);
    *(name + 3) = readl((char *)(memdev->reg_base) + IVSHMEM_REG_IVSHMEM_NAME3);

    snprintf(dev->dev.name, 32, "shm-%s", ivshmem_name);
    device_bind_path(&dev->dev, NULL, &ivshmem_fops, 0666);
    pci_dev_print(dev, "ivshmem_probe %d  (%s)\n", num++, dev->dev.name);

    return 0;
}

static const struct pci_device_id pci_ids[] = {{PCI_VDEVICE(REDHAT_QUMRANET, 0x1110)},
                                               {/* end: all zeroes */}};

/* pci driver glue; this is a "new style" PCI driver module */
static struct pci_driver qemu_ivshmem_pci_driver = {
    .name = "Inter-VM shared memory",
    .id_table = pci_ids,
    .probe = ivshmem_probe,
    .remove = NULL,
};
PCI_DEV_DRIVER_EXPORT(qemu_ivshmem_pci_driver);
