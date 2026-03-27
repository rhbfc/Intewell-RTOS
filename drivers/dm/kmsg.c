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

#include <errno.h>
#include <fcntl.h>
#include <fs/fs.h>
#include <fs/kpoll.h>
#include <klog.h>
#include <stdbool.h>
#include <string.h>
#include <ttos_init.h>

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static ssize_t devkmsg_read(struct file *filep, char *buffer, size_t buflen);
static ssize_t devkmsg_write(struct file *filep, const char *buffer, size_t buflen);
static int devkmsg_poll(struct file *filep, struct kpollfd *fds, bool setup);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct file_operations g_devkmsg_fops = {
    .read = devkmsg_read,   /* read */
    .write = devkmsg_write, /* write */
    .poll = devkmsg_poll    /* poll */
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: devkmsg_read
 ****************************************************************************/

static ssize_t devkmsg_read(struct file *filep, char *buffer, size_t len)
{
    unsigned int pos = filep->f_pos;
    int size = klog_read_msg(buffer, &pos, len, !!(filep->f_oflags & O_NONBLOCK));
    if (size == 0 && !(filep->f_oflags & O_NONBLOCK))
    {
        // return -EAGAIN;
        return 0;
    }
    filep->f_pos = pos;
    return size;
}

/****************************************************************************
 * Name: devkmsg_write
 ****************************************************************************/

static ssize_t devkmsg_write(struct file *filep, const char *buffer, size_t len)
{
    return len;
}

/****************************************************************************
 * Name: devkmsg_poll
 ****************************************************************************/

static int devkmsg_poll(struct file *filep, struct kpollfd *fds, bool setup)
{
    if (setup)
    {
        kpoll_notify(&fds, 1, POLLIN | POLLOUT, NULL);
    }

    return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: devkmsg_register
 *
 * Description:
 *   Register /dev/zero
 *
 ****************************************************************************/

static int devkmsg_register(void)
{
    return register_driver("/dev/kmsg", &g_devkmsg_fops, 0666, NULL);
}
INIT_EXPORT_DRIVER(devkmsg_register, "devkmsg_register");
