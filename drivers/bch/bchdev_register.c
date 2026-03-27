/****************************************************************************
 * drivers/bch/bchdev_register.c
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
#include <stdbool.h>

#include <fs/fs.h>

#include "bch.h"

#define KLOG_TAG "BCHDEV"
#include <klog.h>

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bchdev_register
 *
 * Description:
 *   Setup so that it exports the block driver referenced by 'blkdev' as a
 *   character device 'chardev'
 *
 ****************************************************************************/

int bchdev_register(const char *blkdev, const char *chardev, bool readonly)
{
    void *handle;
    int ret;

    KLOG_D("blkdev=\"%s\" chardev=\"%s\" readonly=%c", blkdev, chardev, readonly ? 'T' : 'F');

    /* Setup the BCH lib functions */

    ret = bchlib_setup(blkdev, readonly, &handle);
    if (ret < 0)
    {
        KLOG_E("bchlib_setup failed: %d", -ret);
        return ret;
    }

    /* Then setup the character device */

    ret = register_driver(chardev, &g_bch_fops, 0666, handle);
    if (ret < 0)
    {
        KLOG_E("register_driver failed: %d", -ret);
        bchlib_teardown(handle);
        handle = NULL;
    }

    return ret;
}
