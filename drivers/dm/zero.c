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
#include <stdbool.h>
#include <string.h>
#include <ttos_init.h>

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static ssize_t devzero_read (struct file *filep, char *buffer, size_t buflen);
static ssize_t devzero_write (struct file *filep, const char *buffer,
                              size_t buflen);
static int devzero_poll (struct file *filep, struct kpollfd *fds, bool setup);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct file_operations g_devzero_fops = {
    .read  = devzero_read,  /* read */
    .write = devzero_write, /* write */
    .poll  = devzero_poll   /* poll */
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: devzero_read
 ****************************************************************************/

static ssize_t devzero_read (struct file *filep, char *buffer, size_t len)
{
    memset (buffer, 0, len);
    return len;
}

/****************************************************************************
 * Name: devzero_write
 ****************************************************************************/

static ssize_t devzero_write (struct file *filep, const char *buffer,
                              size_t len)
{
    return len;
}

/****************************************************************************
 * Name: devzero_poll
 ****************************************************************************/

static int devzero_poll (struct file *filep, struct kpollfd *fds, bool setup)
{
    if (setup)
    {
        kpoll_notify (&fds, 1, POLLIN | POLLOUT, NULL);
    }

    return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: devzero_register
 *
 * Description:
 *   Register /dev/zero
 *
 ****************************************************************************/

static int devzero_register (void)
{
    return register_driver ("/dev/zero", &g_devzero_fops, 0666, NULL);
}
INIT_EXPORT_DRIVER (devzero_register, "devzero_register");
