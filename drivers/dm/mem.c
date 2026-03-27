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

#include <fs/fs.h>
#include <fs/kpoll.h>
#include <ttosProcess.h>
#include <ttos_init.h>

#define KLOG_TAG "MM"
#include <klog.h>

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static ssize_t devmem_read (struct file *filep, char *buffer, size_t buflen);
static ssize_t devmem_write (struct file *filep, const char *buffer,
                             size_t buflen);
static int devmem_poll (struct file *filep, struct kpollfd *fds, bool setup);
static int devmem_mmap (struct file *filep, struct mm_region *map);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct file_operations g_devmem_fops = {
    .read  = devmem_read,  /* read */
    .write = devmem_write, /* write */
    .poll  = devmem_poll,  /* poll */
    .mmap  = devmem_mmap,
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: devmem_read
 ****************************************************************************/

static ssize_t devmem_read (struct file *filep, char *buffer, size_t len)
{
    return len; /* Return EOF */
}

/****************************************************************************
 * Name: devmem_write
 ****************************************************************************/

static ssize_t devmem_write (struct file *filep, const char *buffer, size_t len)
{
    return len; /* Say that everything was written */
}

/****************************************************************************
 * Name: devmem_poll
 ****************************************************************************/

static int devmem_poll (struct file *filep, struct kpollfd *fds, bool setup)
{
    if (setup)
    {
        kpoll_notify (&fds, 1, POLLIN | POLLOUT, NULL);
    }

    return 0;
}

static int devmem_mmap (struct file *filep, struct mm_region *map)
{
    map->priv.p = NULL;
    map->munmap = NULL;
    map->physical_address = ALIGN_DOWN (map->offset, PAGE_SIZE);
    map->mem_attr = MT_DEVICE | MT_USER;

    return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: devmem_register
 *
 * Description:
 *   Register /dev/null
 *
 ****************************************************************************/

static int devmem_register (void)
{
    return register_driver ("/dev/mem", &g_devmem_fops, 0600, NULL);
}
INIT_EXPORT_DRIVER (devmem_register, "devmem_register");
