/****************************************************************************
 *
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

#include <errno.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <fs/fs.h>

#include "pipe_common.h"

#if CONFIG_DEV_FIFO_SIZE > 0

int register_pipedriver (const char *path, const struct file_operations *fops,
                         mode_t mode, void *priv);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct file_operations g_fifo_fops = {
    .open  = pipecommon_open,  /* open */
    .close = pipecommon_close, /* close */
    .read  = pipecommon_read,  /* read */
    .write = pipecommon_write, /* write */
    .ioctl = pipecommon_ioctl, /* ioctl */
    .poll  = pipecommon_poll,  /* poll */
#ifndef CONFIG_DISABLE_PSEUDOFS_OPERATIONS
    .unlink = pipecommon_unlink, /* unlink */
#endif
};

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: ttos_mkfifo
 *
 * Description:
 *   ttos_mkfifo() makes a FIFO device driver file with name 'pathname.' Unlike
 *   Linux, a TTOS FIFO is not a special file type but simply a device
 *   driver instance.  'mode' specifies the FIFO's permissions.
 *
 *   Once the FIFO has been created by ttos_mkfifo(), any thread can open it
 *   for reading or writing, in the same way as an ordinary file. However, it
 *   must have been opened from both reading and writing before input or
 *   output can be performed.  This FIFO implementation will block all
 *   attempts to open a FIFO read-only until at least one thread has opened
 *   the FIFO for  writing.
 *
 *   If all threads that write to the FIFO have closed, subsequent calls to
 *   read() on the FIFO will return 0 (end-of-file).
 *
 *   NOTE: ttos_mkfifo is a special, non-standard, TTOS interface.  Since
 *   the TTOS FIFOs are based in in-memory, circular buffers, the ability
 *   to control the size of those buffers is critical for system tuning.
 *
 * Input Parameters:
 *   pathname - The full path to the FIFO instance to attach to or to create
 *     (if not already created).
 *   mode - Ignored for now
 *   bufsize - The size of the in-memory, circular buffer in bytes.
 *
 * Returned Value:
 *   0 is returned on success; a negated errno value is returned on a
 *   failure.
 *
 ****************************************************************************/

int ttos_mkfifo (const char *pathname, mode_t mode, size_t bufsize)
{
    struct pipe_dev_s *dev;
    int                ret;

    /* Allocate and initialize a new device structure instance */

    dev = pipecommon_allocdev (bufsize);
    if (!dev)
    {
        return -ENOMEM;
    }

    ret = register_pipedriver (pathname, &g_fifo_fops, mode, (void *)dev);
    if (ret != 0)
    {
        pipecommon_freedev (dev);
    }

    return ret;
}

int ttos_fifo_size_get (void)
{
    return CONFIG_DEV_FIFO_SIZE;
}
#endif /* CONFIG_DEV_FIFO_SIZE > 0 */
