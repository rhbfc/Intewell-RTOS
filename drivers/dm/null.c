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
#include <ttos_init.h>

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static ssize_t devnull_read (struct file *filep, char *buffer, size_t buflen);
static ssize_t devnull_write (struct file *filep, const char *buffer,
                              size_t buflen);
static int devnull_poll (struct file *filep, struct kpollfd *fds, bool setup);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct file_operations g_devnull_fops = {
    .read  = devnull_read,  /* read */
    .write = devnull_write, /* write */
    .poll  = devnull_poll   /* poll */
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: devnull_read
 ****************************************************************************/

static ssize_t devnull_read (struct file *filep, char *buffer, size_t len)
{
    return 0; /* Return EOF */
}

/****************************************************************************
 * Name: devnull_write
 ****************************************************************************/

static ssize_t devnull_write (struct file *filep, const char *buffer,
                              size_t len)
{
    return len; /* Say that everything was written */
}

/****************************************************************************
 * Name: devnull_poll
 ****************************************************************************/

static int devnull_poll (struct file *filep, struct kpollfd *fds, bool setup)
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
 * Name: devnull_register
 *
 * Description:
 *   Register /dev/null
 *
 ****************************************************************************/

static int devnull_register (void)
{
    return register_driver ("/dev/null", &g_devnull_fops, 0666, NULL);
}
INIT_EXPORT_DRIVER (devnull_register, "devnull_register");
