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

#define PRINTABLE_FIRST 0x20
#define PRINTABLE_COUNT (0x7f - 0x20)

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static ssize_t devascii_read (struct file *filep, char *buffer, size_t buflen);
static ssize_t devascii_write (struct file *filep, const char *buffer,
                               size_t buflen);
static int devascii_poll (struct file *filep, struct kpollfd *fds, bool setup);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct file_operations g_devascii_fops = {
    .read  = devascii_read,  /* read */
    .write = devascii_write, /* write */
    .poll  = devascii_poll   /* poll */
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: devascii_read
 ****************************************************************************/

static ssize_t devascii_read (struct file *filep, char *buffer, size_t len)
{
    size_t i;
    for (i = 0; i < len; i++)
    {
        buffer[i] = PRINTABLE_FIRST + (filep->f_pos + i) % PRINTABLE_COUNT;

        /* Replace the space character with a newline */

        if (buffer[i] == PRINTABLE_FIRST)
        {
            buffer[i] = '\n';
        }
    }

    filep->f_pos += len;
    return len;
}

/****************************************************************************
 * Name: devascii_write
 ****************************************************************************/

static ssize_t devascii_write (struct file *filep, const char *buffer,
                               size_t len)
{
    return len;
}

/****************************************************************************
 * Name: devascii_poll
 ****************************************************************************/

static int devascii_poll (struct file *filep, struct kpollfd *fds, bool setup)
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
 * Name: devascii_register
 *
 * Description:
 *   Register /dev/ascii
 *
 ****************************************************************************/

static int devascii_register (void)
{
    return register_driver ("/dev/ascii", &g_devascii_fops, 0666, NULL);
}
INIT_EXPORT_DRIVER (devascii_register, "devascii_register");