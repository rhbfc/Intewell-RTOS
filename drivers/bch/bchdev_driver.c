/****************************************************************************
 * drivers/bch/bchdev_driver.c
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

#include <sys/stat.h>
#include <sys/types.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>

#include "bch.h"

#include <fs/fs.h>
#include <fs/ioctl.h>
#include <linux/uaccess.h>
#include <ttos.h>

#include <sys/mount.h>

#define KLOG_TAG "BCHDEV"
#include <klog.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int bch_open(struct file *filep);
static int bch_close(struct file *filep);
static off_t bch_seek(struct file *filep, off_t offset, int whence);
static ssize_t bch_read(struct file *filep, char *buffer, size_t buflen);
static ssize_t bch_write(struct file *filep, const char *buffer, size_t buflen);
static int bch_ioctl(struct file *filep, unsigned int cmd, unsigned long arg);
static int bch_poll(struct file *filep, struct kpollfd *fds, bool setup);
#ifndef CONFIG_DISABLE_PSEUDOFS_OPERATIONS
static int bch_unlink(struct inode *inode);
#endif

/****************************************************************************
 * Public Data
 ****************************************************************************/

const struct file_operations g_bch_fops = {
    .open = bch_open,   /* open */
    .close = bch_close, /* close */
    .read = bch_read,   /* read */
    .write = bch_write, /* write */
    .seek = bch_seek,   /* seek */
    .ioctl = bch_ioctl, /* ioctl */
    NULL,               /* mmap  */
    .poll = bch_poll    /* poll */
#ifndef CONFIG_DISABLE_PSEUDOFS_OPERATIONS
    ,
    .unlink = bch_unlink /* unlink */
#endif
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bch_poll
 ****************************************************************************/

static int bch_poll(struct file *filep, struct kpollfd *fds, bool setup)
{
    if (setup)
    {
        kpoll_notify(&fds, 1, POLLIN | POLLOUT, NULL);
    }

    return 0;
}

/****************************************************************************
 * Name: bch_open
 *
 * Description: Open the block device
 *
 ****************************************************************************/

static int bch_open(struct file *filep)
{
    struct inode *inode = filep->f_inode;
    struct bchlib_s *bch;
    int ret = 0;

    assert(inode && inode->i_private);
    bch = (struct bchlib_s *)inode->i_private;

    /* Increment the reference count */

    ret = TTOS_ObtainMutex(bch->lock, TTOS_WAIT_FOREVER);
    if (ret < 0)
    {
        return ret;
    }

    if (bch->refs == MAX_OPENCNT)
    {
        ret = -EMFILE;
    }
    else
    {
        bch->refs++;
    }

    TTOS_ReleaseMutex(bch->lock);
    return ret;
}

/****************************************************************************
 * Name: bch_close
 *
 * Description: close the block device
 *
 ****************************************************************************/

static int bch_close(struct file *filep)
{
    struct inode *inode = filep->f_inode;
    struct bchlib_s *bch;
    int ret = 0;

    assert(inode && inode->i_private);
    bch = (struct bchlib_s *)inode->i_private;

    /* Get exclusive access */

    ret = TTOS_ObtainMutex(bch->lock, TTOS_WAIT_FOREVER);
    if (ret < 0)
    {
        return ret;
    }

    /* Flush any dirty pages remaining in the cache */

    bchlib_flushsector(bch);

    /* Decrement the reference count (I don't use bchlib_decref() because I
     * want the entire close operation to be atomic wrt other driver
     * operations.
     */

    if (bch->refs == 0)
    {
        ret = -EIO;
    }
    else
    {
        bch->refs--;

        /* If the reference count decremented to zero AND if the character
         * driver has been unlinked, then teardown the BCH device now.
         */

        if (bch->refs == 0 && bch->unlinked)
        {
            /* Tear the driver down now. */

            ret = bchlib_teardown((void *)bch);

            /* bchlib_teardown() would only fail if there are outstanding
             * references on the device.  Since we know that is not true, it
             * should not fail at all.
             */

            assert(ret >= 0);
            if (ret >= 0)
            {
                /* Return without releasing the stale semaphore */

                return 0;
            }
        }
    }

    TTOS_ReleaseMutex(bch->lock);
    return ret;
}

/****************************************************************************
 * Name: bch_seek
 ****************************************************************************/

static off_t bch_seek(struct file *filep, off_t offset, int whence)
{
    struct inode *inode = filep->f_inode;
    struct bchlib_s *bch;
    int64_t newpos64;
    off_t ret;
    uint64_t filesize;

    assert(inode && inode->i_private);

    bch = (struct bchlib_s *)inode->i_private;
    ret = TTOS_ObtainMutex(bch->lock, TTOS_WAIT_FOREVER);
    if (ret < 0)
    {
        return ret;
    }

    filesize = (uint64_t)bch->sectsize * (uint64_t)bch->nsectors;

    /* Determine the new, requested file position */

    switch (whence)
    {
    case SEEK_CUR:
        newpos64 = (int64_t)filep->f_pos + (int64_t)offset;
        break;

    case SEEK_SET:
        newpos64 = (int64_t)offset;
        break;

    case SEEK_END:
        newpos64 = (int64_t)filesize + (int64_t)offset;
        break;

    default:

        /* Return EINVAL if the whence argument is invalid */

        TTOS_ReleaseMutex(bch->lock);
        return -EINVAL;
    }

    /* Opengroup.org:
     *
     *  "The lseek() function shall allow the file offset to be set beyond the
     *   end of the existing data in the file. If data is later written at this
     *   point, subsequent reads of data in the gap shall return bytes with the
     *   value 0 until data is actually written into the gap."
     *
     * We can conform to the first part, but not the second. But return -EINVAL
     * if:
     *
     *  "...the resulting file offset would be negative for a regular file,
     *  block special file, or directory."
     */

    if (newpos64 >= 0)
    {
        if (newpos64 > (int64_t)LONG_MAX)
        {
            ret = -EOVERFLOW;
        }
        else
        {
            filep->f_pos = (off_t)newpos64;
            ret = filep->f_pos;
        }
    }
    else
    {
        ret = -EINVAL;
    }

    TTOS_ReleaseMutex(bch->lock);
    return ret;
}

/****************************************************************************
 * Name: bch_read
 ****************************************************************************/

static ssize_t bch_read(struct file *filep, char *buffer, size_t len)
{
    struct inode *inode = filep->f_inode;
    struct bchlib_s *bch;
    ssize_t ret;

    assert(inode && inode->i_private);
    bch = (struct bchlib_s *)inode->i_private;

    ret = TTOS_ObtainMutex(bch->lock, TTOS_WAIT_FOREVER);
    if (ret < 0)
    {
        return ret;
    }

    ret = bchlib_read(bch, buffer, filep->f_pos, len);
    if (ret > 0)
    {
        filep->f_pos += ret;
    }

    TTOS_ReleaseMutex(bch->lock);
    return ret;
}

/****************************************************************************
 * Name: bch_write
 ****************************************************************************/

static ssize_t bch_write(struct file *filep, const char *buffer, size_t len)
{
    struct inode *inode = filep->f_inode;
    struct bchlib_s *bch;
    ssize_t ret = -EACCES;

    assert(inode && inode->i_private);
    bch = (struct bchlib_s *)inode->i_private;

    if (!bch->readonly)
    {
        ret = TTOS_ObtainMutex(bch->lock, TTOS_WAIT_FOREVER);
        if (ret < 0)
        {
            return ret;
        }

        ret = bchlib_write(bch, buffer, filep->f_pos, len);
        if (ret > 0)
        {
            filep->f_pos += ret;
        }

        TTOS_ReleaseMutex(bch->lock);
    }

    return ret;
}

#ifndef BLKGETSIZE64
#define BLKGETSIZE64 _IOR(0x12, 114, size_t)
#endif

#ifndef BLKFLSBUF
#define BLKFLSBUF _IO(0x12, 97)
#endif

#ifndef BLKDISCARD
#define BLKDISCARD _IO(0x12, 119)
#endif

// static_assert(BLKGETSIZE64 == 0x80081272, "BLKGETSIZE64 is not 0x80081272");
//  static_assert(BLKFLSBUF == 0x80081261, "BLKFLSBUF is not 0x80081261");
//  static_assert(BLKDISCARD == 0x80081277, "BLKDISCARD is not 0x80081277");

struct hd_geometry
{
    unsigned char heads;
    unsigned char sectors;
    unsigned short cylinders;
    unsigned long start;
};

#define HDIO_GETGEO 0x0301 /* get device geometry */

/****************************************************************************
 * Name: bch_ioctl
 *
 * Description:
 *   Handle IOCTL commands
 *
 ****************************************************************************/

static int bch_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
    struct inode *inode = filep->f_inode;
    struct bchlib_s *bch;
    int ret = -ENOTTY;
    uint64_t total_bytes;

    assert(inode && inode->i_private);
    bch = (struct bchlib_s *)inode->i_private;
    total_bytes = (uint64_t)bch->nsectors * (uint64_t)bch->sectsize;

    /* Process the call according to the command */

    switch ((uintptr_t)cmd)
    {
    case FIOCLEX:
    {
        filep->f_oflags |= O_CLOEXEC;
        ret = 0;
        break;
    }
    case FIONCLEX:
    {
        filep->f_oflags &= ~O_CLOEXEC;
        ret = 0;
        break;
    }
        /* This isa request to get the private data structure */
    case BLKGETSIZE64:
    {
        if (arg == 0)
        {
            ret = -EINVAL;
            break;
        }

        ret = copy_to_user((void __user *)(uintptr_t)arg, &total_bytes, sizeof(total_bytes));
    }
    break;
    case BLKGETSIZE:
    {
        uint64_t size_in_512b_sectors = total_bytes / 512U;

        if (arg == 0)
        {
            ret = -EINVAL;
            break;
        }

        if (size_in_512b_sectors > ULONG_MAX)
        {
            ret = -EOVERFLOW;
            break;
        }

        ret = put_user((unsigned long)size_in_512b_sectors, (unsigned long *)arg);
    }
    break;
    case BLKSSZGET:
    {
        if (arg == 0)
        {
            ret = -EINVAL;
            break;
        }

        if (bch->sectsize > INT_MAX)
        {
            ret = -EOVERFLOW;
            break;
        }

        ret = put_user((int)bch->sectsize, (int *)arg);
    }
    break;
    case HDIO_GETGEO:
    {
        struct hd_geometry geo = {0};
        uint64_t cylinders;

        if (arg == 0)
        {
            ret = -EINVAL;
            break;
        }

        geo.heads = 255;
        geo.sectors = 63;
        cylinders = (uint64_t)bch->nsectors / ((uint64_t)geo.heads * (uint64_t)geo.sectors);
        if (cylinders > USHRT_MAX)
        {
            geo.cylinders = USHRT_MAX;
        }
        else
        {
            geo.cylinders = (unsigned short)cylinders;
        }
        geo.start = 0;

        ret = copy_to_user((void __user *)(uintptr_t)arg, &geo, sizeof(geo));
    }
    break;
    case DIOC_GETPRIV:
    {
        ret = TTOS_ObtainMutex(bch->lock, TTOS_WAIT_FOREVER);
        if (ret < 0)
        {
            return ret;
        }

        if (arg == 0 || bch->refs == MAX_OPENCNT)
        {
            ret = -EINVAL;
        }
        else
        {
            bch->refs++;
            ret = copy_to_user((void __user *)(uintptr_t)arg, &bch, sizeof(bch));
            if (ret < 0)
            {
                bch->refs--;
            }
        }

        TTOS_ReleaseMutex(bch->lock);
    }
    break;

        /* This is a required to return the geometry of the underlying block
         * driver.
         */

    case BIOC_GEOMETRY:
    {
        struct geometry geo = {0};

        if (arg == 0)
        {
            ret = -EINVAL;
            break;
        }

        assert(bch->inode && bch->inode->u.i_bops && bch->inode->u.i_bops->geometry);

        ret = bch->inode->u.i_bops->geometry(bch->inode, &geo);
        if (ret < 0)
        {
            KLOG_E("geometry failed: %d", -ret);
        }
        else if (!geo.geo_available)
        {
            KLOG_E("geometry failed: %d", -ret);
            ret = -ENODEV;
        }
        else
        {
            ret = copy_to_user((void __user *)(uintptr_t)arg, &geo, sizeof(geo));
        }
    }
    break;

    case BIOC_FLUSH:
    {
        /* Flush any dirty pages remaining in the cache */

        ret = bchlib_flushsector(bch);
    }
    break;

#ifdef CONFIG_BCH_ENCRYPTION
        /* This is a request to set the encryption key? */

    case DIOC_SETKEY:
    {
        if (arg == 0)
        {
            ret = -EINVAL;
        }
        else
        {
            ret = copy_from_user(bch->key, (void __user *)(uintptr_t)arg,
                                 CONFIG_BCH_ENCRYPTION_KEY_SIZE);
        }
    }
    break;
#endif

        /* Otherwise, pass the IOCTL command on to the contained block
         * driver.
         */

    default:
    {
        struct inode *bchinode = bch->inode;

        /* Does the block driver support the ioctl method? */

        if (bchinode->u.i_bops->ioctl != NULL)
        {
            ret = bchinode->u.i_bops->ioctl(bchinode, cmd, arg);
        }
    }
    break;
    }

    return ret;
}

/****************************************************************************
 * Name: bch_unlink
 *
 * Handle unlinking of the BCH device
 *
 ****************************************************************************/

#ifndef CONFIG_DISABLE_PSEUDOFS_OPERATIONS
static int bch_unlink(struct inode *inode)
{
    struct bchlib_s *bch;
    int ret = 0;

    assert(inode && inode->i_private);
    bch = (struct bchlib_s *)inode->i_private;

    /* Get exclusive access to the BCH device */

    ret = TTOS_ObtainMutex(bch->lock, TTOS_WAIT_FOREVER);
    if (ret < 0)
    {
        return ret;
    }

    /* Indicate that the driver has been unlinked */

    bch->unlinked = true;

    /* If there are no open references to the driver then teardown the BCH
     * device now.
     */

    if (bch->refs == 0)
    {
        /* Tear the driver down now. */

        ret = bchlib_teardown((void *)bch);

        /* bchlib_teardown() would only fail if there are outstanding
         * references on the device.  Since we know that is not true, it
         * should not fail at all.
         */

        assert(ret >= 0);
        if (ret >= 0)
        {
            /* Return without releasing the stale semaphore */

            return 0;
        }
    }

    TTOS_ReleaseMutex(bch->lock);
    return ret;
}
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/
