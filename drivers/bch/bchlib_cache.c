/****************************************************************************
 * drivers/bch/bchlib_cache.c
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
#include <sys/types.h>

#include "bch.h"

#if defined(CONFIG_BCH_ENCRYPTION)
#include <crypto/crypto.h>
#endif

#define KLOG_TAG "BCHLIB"
#include <klog.h>

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bch_xor
 ****************************************************************************/

#if defined(CONFIG_BCH_ENCRYPTION)
static void bch_xor(uint32_t *R, uint32_t *A, uint32_t *B)
{
    R[0] = A[0] ^ B[0];
    R[1] = A[1] ^ B[1];
    R[2] = A[2] ^ B[2];
    R[3] = A[3] ^ B[3];
}
#endif

/****************************************************************************
 * Name: bch_cypher
 ****************************************************************************/

#if defined(CONFIG_BCH_ENCRYPTION)
static int bch_cypher(struct bchlib_s *bch, int encrypt)
{
    int blocks = bch->sectsize / 16;
    uint32_t *buffer = (uint32_t *)bch->buffer;
    int i;

    for (i = 0; i < blocks; i++, buffer += 16 / sizeof(uint32_t))
    {
        uint32_t T[4];
        uint32_t X[4] = {bch->sector, 0, 0, i};

        aes_cypher(X, X, 16, NULL, bch->key, CONFIG_BCH_ENCRYPTION_KEY_SIZE, AES_MODE_ECB,
                   CYPHER_ENCRYPT);

        /* Xor-Encrypt-Xor */

        bch_xor(T, X, buffer);
        aes_cypher(T, T, 16, NULL, bch->key, CONFIG_BCH_ENCRYPTION_KEY_SIZE, AES_MODE_ECB, encrypt);
        bch_xor(buffer, X, T);
    }

    return OK;
}
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bchlib_flushsector
 *
 * Description:
 *   Flush the current contents of the sector buffer (if dirty)
 *
 * Assumptions:
 *   Caller must assume mutual exclusion
 *
 ****************************************************************************/

int bchlib_flushsector(struct bchlib_s *bch)
{
    struct inode *inode;
    ssize_t ret = 0;

    /* Check if the sector has been modified and is out of synch with the
     * media.
     */

    if (bch->dirty)
    {
        inode = bch->inode;

#if defined(CONFIG_BCH_ENCRYPTION)
        /* Encrypt data as necessary */

        bch_cypher(bch, CYPHER_ENCRYPT);
#endif

        /* Write the sector to the media */

        ret = inode->u.i_bops->write(inode, bch->buffer, bch->sector, 1);
        if (ret < 0)
        {
            KLOG_E("Write failed: %zd", ret);
            return (int)ret;
        }

#if defined(CONFIG_BCH_ENCRYPTION)
        /* Computation overhead to save memory for extra sector buffer
         * TODO: Add configuration switch for extra sector buffer
         */

        bch_cypher(bch, CYPHER_DECRYPT);
#endif

        /* The sector is now in sync with the media */

        bch->dirty = false;
    }

    return (int)ret;
}

/****************************************************************************
 * Name: bchlib_readsector
 *
 * Description:
 *   Flush the current contents of the sector buffer (if dirty)
 *
 * Assumptions:
 *   Caller must assume mutual exclusion
 *
 ****************************************************************************/

int bchlib_readsector(struct bchlib_s *bch, size_t sector)
{
    struct inode *inode;
    ssize_t ret = 0;

    if (bch->sector != sector)
    {
        inode = bch->inode;

        ret = bchlib_flushsector(bch);
        if (ret < 0)
        {
            KLOG_E("Flush failed: %zd", ret);
            return (int)ret;
        }

        bch->sector = (size_t)-1;

        ret = inode->u.i_bops->read(inode, bch->buffer, sector, 1);
        if (ret < 0)
        {
            KLOG_E("Read failed: %zd", ret);
            return (int)ret;
        }

        bch->sector = sector;
#if defined(CONFIG_BCH_ENCRYPTION)
        bch_cypher(bch, CYPHER_DECRYPT);
#endif
    }

    return (int)ret;
}
