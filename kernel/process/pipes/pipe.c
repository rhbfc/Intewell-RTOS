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

#include <sys/types.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <fs/fs.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <ttos.h>
#include <ttosUtils.h>
#include <ttos_init.h>
#include <unistd.h>

#include "pipe_common.h"

#if CONFIG_DEV_PIPE_SIZE > 0

int register_pipedriver(const char *path, const struct file_operations *fops, mode_t mode,
                        void *priv);

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int pipe_close(struct file *filep);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct file_operations g_pipe_fops = {
    pipecommon_open,  /* open */
    pipe_close,       /* close */
    pipecommon_read,  /* read */
    pipecommon_write, /* write */
    NULL,             /* seek */
    pipecommon_ioctl, /* ioctl */
    NULL,             /* mmap  */
    pipecommon_poll   /* poll */
};

static MUTEX_ID g_pipelock;
static int g_pipeno;

int pipe_lock_init(void)
{
    T_TTOS_ReturnCode ret = TTOS_CreateMutex(1, 0, &g_pipelock);
    return (ret == TTOS_OK ? 0 : -ENOMEM);
}

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: pipe_allocate
 ****************************************************************************/
static inline int pipe_allocate(void)
{
    int ret;

    ret = TTOS_ObtainMutex(g_pipelock, TTOS_WAIT_FOREVER);
    if (ret < 0)
    {
        return ret;
    }

    ret = g_pipeno++;
    if (g_pipeno < 0)
    {
        g_pipeno = 0;
    }

    TTOS_ReleaseMutex(g_pipelock);

    return ret;
}

/****************************************************************************
 * Name: pipe_close
 ****************************************************************************/

static int pipe_close(struct file *filep)
{
    struct inode *inode = filep->f_inode;
    struct pipe_dev_s *dev = inode->i_private;
    int ret;

    /* Perform common close operations */
    ret = pipecommon_close(filep);
    if (ret == 0 && inode->i_crefs == 1)
    {
        /* Release the pipe when there are no further open references to it. */

        pipecommon_freedev(dev);
    }

    return ret;
}

/****************************************************************************
 * Name: pipe_register
 ****************************************************************************/

static int pipe_register(size_t bufsize, int flags, char *devname, size_t namesize)
{
    struct pipe_dev_s *dev;
    int pipeno;
    int ret;

    /* Allocate a minor number for the pipe device */

    pipeno = pipe_allocate();
    if (pipeno < 0)
    {
        return pipeno;
    }

    /* Create a pathname to the pipe device */

    snprintf(devname, namesize, CONFIG_DEV_PIPE_VFS_PATH "%d", pipeno);

    /* Allocate and initialize a new device structure instance */

    dev = pipecommon_allocdev(bufsize);
    if (dev == NULL)
    {
        return -ENOMEM;
    }

    /* Register the pipe device */

    ret = register_pipedriver(devname, &g_pipe_fops, 0666, (void *)dev);
    if (ret != 0)
    {
        pipecommon_freedev(dev);
    }

    return ret;
}

int register_pipedriver(const char *path, const struct file_operations *fops, mode_t mode,
                        void *priv)
{
    struct inode *node;
    int ret;

    ret = inode_lock();
    if (ret < 0)
    {
        return ret;
    }

    ret = inode_reserve(path, mode, &node);
    if (ret >= 0)
    {
        /* We have it, now populate it with driver specific information.
         * NOTE that the initial reference count on the new inode is zero.
         */

        INODE_SET_PIPE(node);

        node->u.i_ops = fops;
        node->i_private = priv;
        ret = 0;
    }

    inode_unlock();
    return ret;
}

int unregister_pipedriver(const char *path)
{
    int ret;

    ret = inode_lock();
    if (ret >= 0)
    {
        ret = inode_remove(path);
        inode_unlock();
    }

    return ret;
}

int file_pipe(struct file *filep[2], size_t bufsize, int flags)
{
    char devname[32];
    int nonblock = !!(flags & O_NONBLOCK);
    int ret;

    ret = pipe_register(bufsize, flags, devname, sizeof(devname));
    if (ret < 0)
    {
        return ret;
    }

    ret = file_open(filep[1], devname, O_WRONLY | O_NONBLOCK | flags);
    if (ret < 0)
    {
        goto errout_with_driver;
    }

    if (!nonblock)
    {
        ret = file_ioctl(filep[1], FIONBIO, &nonblock);
        if (ret < 0)
        {
            goto errout_with_driver;
        }
    }

    ret = file_open(filep[0], devname, O_RDONLY | flags);
    if (ret < 0)
    {
        goto errout_with_wrfd;
    }

    unregister_pipedriver(devname);
    return 0;

errout_with_wrfd:
    file_close(filep[1]);

errout_with_driver:
    unregister_pipedriver(devname);
    return ret;
}

int vfs_pipe2(int fd[2], int flags)
{
    char devname[32];
    int nonblock = !!(flags & O_NONBLOCK);
    int ret;

    ret = pipe_register(CONFIG_DEV_PIPE_SIZE, flags, devname, sizeof(devname));
    if (ret < 0)
    {
        return -1;
    }

    fd[1] = vfs_open(devname, O_WRONLY | O_NONBLOCK | flags);
    if (fd[1] < 0)
    {
        goto errout_with_driver;
    }

    if (!nonblock)
    {
        ret = vfs_ioctl(fd[1], FIONBIO, &nonblock);
        if (ret < 0)
        {
            goto errout_with_driver;
        }
    }

    fd[0] = vfs_open(devname, O_RDONLY | flags);
    if (fd[0] < 0)
    {
        goto errout_with_wrfd;
    }

    unregister_pipedriver(devname);
    return 0;

errout_with_wrfd:
    close(fd[1]);

errout_with_driver:
    unregister_pipedriver(devname);
    return -1;
}

INIT_EXPORT_SYS(pipe_lock_init, "pipe_lock_init");
#endif /* CONFIG_DEV_PIPE_SIZE > 0 */
