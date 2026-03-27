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
#include <fs/fs.h>
#include <fs/kpoll.h>
#include <ttos_init.h>
#include <stdlib.h>
#include <string.h>
#include <system/macros.h>
#include <time.h>

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static ssize_t devurand_read (struct file *filep, char *buffer, size_t buflen);
static ssize_t devurand_write (struct file *filep, const char *buffer,
                               size_t buflen);
static int devurand_poll (struct file *filep, struct kpollfd *fds, bool setup);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct file_operations g_urand_fops = {
    .read  = devurand_read,  /* read */
    .write = devurand_write, /* write */
    .poll  = devurand_poll   /* poll */
};

static inline uint32_t do_congruential (void)
{
    /* REVISIT:  We could probably generate a 32-bit value with a single
     * call to nrand().
     */

    return (uint32_t)rand () << (uint32_t)rand ();
}

/****************************************************************************
 * Name: devurand_read
 ****************************************************************************/

static ssize_t devurand_read (struct file *filep, char *buffer, size_t len)
{
    size_t   n;
    uint32_t rnd;

    n = len;

    /* Align buffer pointer to 4-byte boundary */

    if (((uintptr_t)buffer & 0x03) != 0)
    {
        /* Generate a pseudo random number */

        rnd = do_congruential ();

        while (((uintptr_t)buffer & 0x03) != 0)
        {
            if (n <= 0)
            {
                return len;
            }

            *buffer++ = rnd & 0xff;
            rnd >>= 8;
            --n;
        }
    }

    /* Stuff buffer with PRNGs 4 bytes at a time */

    while (n >= 4)
    {
        *(uint32_t *)buffer = do_congruential ();
        buffer += 4;
        n -= 4;
    }

    /* Stuff remaining 1, 2, or 3 bytes */

    if (n > 0)
    {
        /* Generate a pseudo random number */

        rnd = do_congruential ();

        do
        {
            *buffer++ = rnd & 0xff;
            rnd >>= 8;
        } while (--n > 0);
    }

    return len;
}

/****************************************************************************
 * Name: devurand_write
 ****************************************************************************/

static ssize_t devurand_write (struct file *filep, const char *buffer,
                               size_t len)
{
    /* Write can be used to re-seed the PRNG state. */

    unsigned int seed = 0;

    len = min (len, sizeof (unsigned int));
    memcpy (&seed, buffer, len);
    srand (seed);
    return len;
}

/****************************************************************************
 * Name: devurand_poll
 ****************************************************************************/

static int devurand_poll (struct file *filep, struct kpollfd *fds, bool setup)
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
 * Name: devurandom_register
 *
 * Description:
 *   Register /dev/urandom
 *
 ****************************************************************************/

static int devurandom_register (void)
{
    /* Seed the PRNG */
    struct timespec tp;
    clock_gettime(CLOCK_REALTIME, &tp);

    srand(tp.tv_nsec);
    register_driver ("/dev/random", &g_urand_fops, 0666, NULL);
    return register_driver ("/dev/urandom", &g_urand_fops, 0666, NULL);
}
INIT_EXPORT_DRIVER (devurandom_register, "devurandom_register");
