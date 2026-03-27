/****************************************************************************
 * drivers/bch/bchlib_setup.c
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

#include <sys/types.h>

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>

#include "bch.h"
#include <fs/fs.h>
#include <ttos.h>

#define KLOG_TAG "BCHLIB"
#include <klog.h>

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bchlib_setup
 *
 * Description:
 *   Setup so that the block driver referenced by 'blkdev' can be accessed
 *   similar to a character device.
 *
 ****************************************************************************/

int bchlib_setup(const char *blkdev, bool readonly, void **handle)
{
    struct bchlib_s *bch;
    struct geometry geo;
    int ret;

    assert(blkdev);

    /* Allocate the BCH state structure */

    bch = (struct bchlib_s *)calloc( 1, sizeof(struct bchlib_s));
    if (!bch)
    {
        KLOG_E("Failed to allocate BCH structure");
        return -ENOMEM;
    }

    /* Open the block driver */

    ret = open_blockdriver(blkdev, readonly ? 1 : 0, &bch->inode);
    if (ret < 0)
    {
        KLOG_E("Failed to open driver %s: %d", blkdev, -ret);
        goto errout_with_bch;
    }

    assert(bch->inode && bch->inode->u.i_bops && bch->inode->u.i_bops->geometry);

    ret = bch->inode->u.i_bops->geometry(bch->inode, &geo);
    if (ret < 0)
    {
        KLOG_E("geometry failed: %d", -ret);
        goto errout_with_bch;
    }

    if (!geo.geo_available)
    {
        KLOG_E("geometry failed: %d", -ret);
        ret = -ENODEV;
        goto errout_with_bch;
    }

    if (!readonly && (!bch->inode->u.i_bops->write || !geo.geo_writeenabled))
    {
        KLOG_E("write access not supported");
        ret = -EACCES;
        goto errout_with_bch;
    }

    /* Save the geometry info and complete initialization of the structure */

    TTOS_CreateMutex(1, 0, &bch->lock);
    bch->nsectors = geo.geo_nsectors;
    bch->sectsize = geo.geo_sectorsize;
    bch->sector = (size_t)-1;
    bch->readonly = readonly;

    /* Allocate the sector I/O buffer */

#if CONFIG_BCH_BUFFER_ALIGNMENT != 0
    bch->buffer = memalign(CONFIG_BCH_BUFFER_ALIGNMENT, bch->sectsize);
#else
    bch->buffer = malloc(bch->sectsize);
#endif
    if (!bch->buffer)
    {
        KLOG_E("Failed to allocate sector buffer");
        ret = -ENOMEM;
        goto errout_with_bch;
    }

    *handle = bch;
    return 0;

errout_with_bch:
    free(bch);
    return ret;
}
