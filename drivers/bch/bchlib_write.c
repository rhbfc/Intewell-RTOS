/****************************************************************************
 * drivers/bch/bchlib_write.c
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

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

#include <fs/fs.h>

#include "bch.h"

#define KLOG_TAG "BCHLIB"
#include <klog.h>

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bchlib_write
 *
 * Description:
 *   Write to the block device set-up by bchlib_setup as if it were a
 *   character device.
 *
 ****************************************************************************/

ssize_t bchlib_write(void *handle, const char *buffer, size_t offset, size_t len)
{
    struct bchlib_s *bch = (struct bchlib_s *)handle;
    size_t nsectors;
    size_t sector;
    uint16_t sectoffset;
    size_t nbytes;
    size_t byteswritten;
    int ret;

    /* Get rid of this special case right away */

    if (len < 1)
    {
        return 0;
    }

    /* Convert the file position into a sector number and offset. */

    sector = offset / bch->sectsize;
    sectoffset = offset - sector * bch->sectsize;

    if (sector >= bch->nsectors)
    {
        return -EFBIG;
    }

    /* Write the initial partial sector */

    byteswritten = 0;
    if (sectoffset > 0)
    {
        /* Read the full sector into the sector buffer */

        ret = bchlib_readsector(bch, sector);
        if (ret < 0)
        {
            return ret;
        }

        /* Copy the tail end of the sector from the user buffer */

        if (sectoffset + len > bch->sectsize)
        {
            nbytes = bch->sectsize - sectoffset;
        }
        else
        {
            nbytes = len;
        }

        memcpy(&bch->buffer[sectoffset], buffer, nbytes);
        bch->dirty = true;

        /* Adjust pointers and counts */

        sector++;

        if (sector >= bch->nsectors)
        {
            return nbytes;
        }

        byteswritten = nbytes;
        buffer += nbytes;
        len -= nbytes;
    }

    /* Then write all of the full sectors following the partial sector
     * directly from the user buffer.
     */

    if (bch->sectsize == 0)
    {
        return -EINVAL;
    }

    if (len >= bch->sectsize)
    {
        nsectors = len / bch->sectsize;
        if (sector + nsectors > bch->nsectors)
        {
            nsectors = bch->nsectors - sector;
        }

        /* Flush the dirty sector to keep the sector sequence */

        ret = bchlib_flushsector(bch);
        if (ret < 0)
        {
            KLOG_E("Flush failed: %d", ret);
            return ret;
        }

        /* Write the contiguous sectors */

        ret = bch->inode->u.i_bops->write(bch->inode, (uint8_t *)buffer, sector, nsectors);
        if (ret < 0)
        {
            KLOG_E("Write failed: %d", ret);
            return ret;
        }

        /* Adjust pointers and counts */

        sector += nsectors;
        nbytes = nsectors * bch->sectsize;
        byteswritten += nbytes;

        if (sector >= bch->nsectors)
        {
            return byteswritten;
        }

        buffer += nbytes;
        len -= nbytes;
    }

    /* Then write any partial final sector */

    if (len > 0)
    {
        /* Read the sector into the sector buffer */

        ret = bchlib_readsector(bch, sector);
        if (ret < 0)
        {
            return ret;
        }

        /* Copy the head end of the sector from the user buffer */

        memcpy(bch->buffer, buffer, len);
        bch->dirty = true;

        /* Adjust counts */

        byteswritten += len;
    }

    return byteswritten;
}
