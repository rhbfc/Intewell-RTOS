/****************************************************************************
 * drivers/pty/ptmx.c
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

/**
 * @file    drivers/pty/ptmx.c
 * @author  wangqinglin
 * @brief
 * @version 3.0.0
 * @date    2024-07-30
 *
 * @note    基于 Apache NuttX 对应 PTY/PTMX 实现移植到当前 RTOS。
 * @note    已针对 VFS、pipe、初始化框架和平台环境进行修改。
 *
 * 科东(广州)软件科技有限公司 版权所有
 * @copyright Copyright (C) 2023 Intewell Inc. All Rights Reserved.
 */

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <fs/fs.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <ttos.h>
#include <unistd.h>
#include <ttos_init.h>

#include "pty.h"

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

#ifndef CONFIG_PTY_MAXPTY
#define CONFIG_PTY_MAXPTY 32
#endif

#if CONFIG_PTY_MAXPTY > 256
#define CONFIG_PTY_MAXPTY 256
#endif

#define PTY_MAX   ((CONFIG_PTY_MAXPTY + 31) & ~31)
#define INDEX_MAX (PTY_MAX >> 5)

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* PTMX device state */

struct ptmx_dev_s {
    MUTEX_ID px_lock;                /* Supports mutual exclusion */
    uint8_t  px_next;                /* Next minor number to allocate */
    uint32_t px_alloctab[INDEX_MAX]; /* Set of allocated PTYs */
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int     ptmx_open (struct file *filep);
static ssize_t ptmx_read (struct file *filep, char *buffer, size_t buflen);
static ssize_t ptmx_write (struct file *filep, const char *buffer,
                           size_t buflen);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct file_operations g_ptmx_fops = {
    .open  = ptmx_open,  /* open */
    .read  = ptmx_read,  /* read */
    .write = ptmx_write, /* write */
};

static struct ptmx_dev_s g_ptmx;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: ptmx_minor_allocate
 *
 * Description:
 *   Allocate a new unique PTY minor number.
 *
 * Assumptions:
 *   Caller hold the px_lock
 *
 ****************************************************************************/

static int ptmx_minor_allocate (void)
{
    uint8_t startaddr = g_ptmx.px_next;
    uint8_t minor;
    int     index;
    int     bitno;

    /* Loop until we find a valid device address */

    for (;;)
    {
        /* Try the next device address */

        minor = g_ptmx.px_next;
        if (g_ptmx.px_next >= PTY_MAX)
        {
            g_ptmx.px_next = 0;
        }
        else
        {
            g_ptmx.px_next++;
        }

        /* Is this address already allocated? */

        index = minor >> 5;
        bitno = minor & 31;
        if ((g_ptmx.px_alloctab[index] & (1 << bitno)) == 0)
        {
            /* No... allocate it now */

            g_ptmx.px_alloctab[index] |= (1 << bitno);
            return (int)minor;
        }

        /* This address has already been allocated.  The following logic will
         * prevent (unexpected) infinite loops.
         */

        if (startaddr == minor)
        {
            /* We are back where we started... the are no free device address */

            return -ENOMEM;
        }
    }
}

/****************************************************************************
 * Name: ptmx_open
 ****************************************************************************/

static int ptmx_open (struct file *filep)
{
    struct file temp;
    char        devname[32];
    int         minor;
    int         ret;

    /* Get exclusive access */

    ret = TTOS_ObtainMutex (g_ptmx.px_lock, TTOS_WAIT_FOREVER);
    ;
    if (ret < 0)
    {
        return ret;
    }

    /* Allocate a PTY minor */

    minor = ptmx_minor_allocate ();
    if (minor < 0)
    {
        ret = minor;
        goto errout_with_lock;
    }

    /* Create the master slave pair.  This should create:
     *
     *   Slave device:  /dev/pts/N
     *   Master device: /dev/ptyN
     *
     * Where N=minor
     */

    ret = pty_register2 (minor, true);
    if (ret < 0)
    {
        goto errout_with_minor;
    }

    /* Open the master device:  /dev/ptyN, where N=minor */

    snprintf (devname, sizeof (devname), "/dev/pty%d", minor);
    memcpy (&temp, filep, sizeof (temp));
    ret = file_open (filep, devname, O_RDWR | O_CLOEXEC);
    assert (ret >= 0); /* file_open() should never fail */

    /* Close the multiplexor device: /dev/ptmx */

    ret = file_close (&temp);
    assert (ret >= 0); /* file_close() should never fail */

    /* Now unlink the master.  This will remove it from the VFS namespace,
     * the driver will still persist because of the open count on the
     * driver.
     */

    ret = unregister_driver (devname);
    assert (ret >= 0
            || ret == -EBUSY); /* unregister_driver() should never fail */

    TTOS_ReleaseMutex (g_ptmx.px_lock);
    return 0;

errout_with_minor:
    ptmx_minor_free (minor);

errout_with_lock:
    TTOS_ReleaseMutex (g_ptmx.px_lock);
    return ret;
}

/****************************************************************************
 * Name: ptmx_read
 ****************************************************************************/

static ssize_t ptmx_read (struct file *filep, char *buffer, size_t len)
{
    return 0; /* Return EOF */
}

/****************************************************************************
 * Name: ptmx_write
 ****************************************************************************/

static ssize_t ptmx_write (struct file *filep, const char *buffer, size_t len)
{
    return len; /* Say that everything was written */
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: ptmx_register
 *
 * Input Parameters:
 *   None
 *
 * Description:
 *   Register the master pseudo-terminal multiplexor device at /dev/ptmx
 *
 * Returned Value:
 *   Zero ( 0) is returned on success; a negated errno value is returned on
 *   any failure.
 *
 ****************************************************************************/

int ptmx_register (void)
{
    /* Register the PTMX driver */
    TTOS_CreateMutex (1, 0, &g_ptmx.px_lock);
    return register_driver ("/dev/ptmx", &g_ptmx_fops, 0666, NULL);
}
INIT_EXPORT_DRIVER(ptmx_register, "register ptmx");
/****************************************************************************
 * Name: ptmx_minor_free
 *
 * Description:
 *   De-allocate a PTY minor number.
 *
 * Assumptions:
 *   Caller hold the px_lock
 *
 ****************************************************************************/

void ptmx_minor_free (uint8_t minor)
{
    int index;
    int bitno;

    TTOS_ObtainMutex (g_ptmx.px_lock, TTOS_WAIT_FOREVER);

    /* Free the address by clearing the associated bit in the px_alloctab[]; */

    index = minor >> 5;
    bitno = minor & 31;

    assert ((g_ptmx.px_alloctab[index] & (1 << bitno)) != 0);
    g_ptmx.px_alloctab[index] &= ~(1 << bitno);

    /* Reset the next pointer if the one just released has a lower value */

    if (minor < g_ptmx.px_next)
    {
        g_ptmx.px_next = minor;
    }

    TTOS_ReleaseMutex (g_ptmx.px_lock);
}
