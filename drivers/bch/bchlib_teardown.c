/****************************************************************************
 * drivers/bch/bchlib_teardown.c
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
#include <stdlib.h>

#include "bch.h"
#include <fs/fs.h>
#include <ttos.h>

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bchlib_teardown
 *
 * Description:
 *   Setup so that the block driver referenced by 'blkdev' can be accessed
 *   similar to a character device.
 *
 ****************************************************************************/

int bchlib_teardown (void *handle)
{
    struct bchlib_s *bch = (struct bchlib_s *)handle;

    assert (handle);

    /* Check that there are not outstanding reference counts on the object */

    if (bch->refs > 0)
    {
        return -EBUSY;
    }

    /* Flush any pending data to the block driver */

    bchlib_flushsector (bch);

    /* Close the block driver */

    close_blockdriver (bch->inode);

    /* Free the BCH state structure */

    if (bch->buffer)
    {
        free (bch->buffer);
    }
	
	TTOS_ReleaseMutex (bch->lock);
    TTOS_DeleteMutex (bch->lock);
	
    free (bch);
	
    return 0;
}
