/****************************************************************************
 * drivers/bch/bchdev_unregister.c
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

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <fs/fs.h>
#include <fs/ioctl.h>

#include "bch.h"

#define KLOG_TAG "BCHDEV"
#include <klog.h>

#include <spinlock.h>
static DEFINE_SPINLOCK(bchdev_unregister_lock);

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bchdev_unregister
 *
 * Description:
 *   Unregister character driver access to a block device that was created
 *   by a previous call to bchdev_register().
 *
 ****************************************************************************/

int bchdev_unregister(const char *chardev)
{
    struct bchlib_s *bch;
    struct file filestruct;
    int ret;
    long flags;

    /* Sanity check */

#ifdef CONFIG_DEBUG_FEATURES
    if (!chardev)
    {
        return -EINVAL;
    }
#endif

    /* Open the character driver associated with chardev */

    ret = file_open(&filestruct, chardev, O_RDONLY);
    if (ret < 0)
    {
        KLOG_E("Failed to open %s: %d", chardev, ret);
        return ret;
    }

    /* Get a reference to the internal data structure.  On success, we
     * will hold a reference count on the state structure.
     */

    ret = file_ioctl(&filestruct, DIOC_GETPRIV, (unsigned long)((uintptr_t)&bch));
    file_close(&filestruct);

    if (ret < 0)
    {
        KLOG_E("ioctl failed: %d", ret);
        return ret;
    }

    /* Lock out context switches.  If there are no other references
     * and no context switches, then we can assume that we can safely
     * teardown the driver.
     */

    spin_lock_irqsave(&bchdev_unregister_lock, flags);

    /* Check if the internal structure is non-busy (we hold one reference). */

    if (bch->refs > 1)
    {
        ret = -EBUSY;
        goto errout_with_lock;
    }

    /* Unregister the driver (this cannot suspend or we lose our non-preemptive
     * state!).  Once the driver is successfully unregistered, we can assume
     * we have exclusive access to the state instance.
     */

    ret = unregister_driver(chardev);
    if (ret < 0)
    {
        goto errout_with_lock;
    }

    spin_unlock_irqrestore(&bchdev_unregister_lock, flags);

    /* Release the internal structure */

    bch->refs = 0;
    return bchlib_teardown(bch);

errout_with_lock:
    bch->refs--;
    spin_unlock_irqrestore(&bchdev_unregister_lock, flags);
    return ret;
}
