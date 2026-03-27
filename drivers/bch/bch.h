/****************************************************************************
 * drivers/bch/bch.h
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

/****************************************************************************
 * 本文件基于 Apache NuttX 对应 bch 实现移植到当前 RTOS，
 * 并已针对本地接口、同步原语和平台环境进行了修改。
 ****************************************************************************/

#ifndef __DRIVERS_BCH_BCH_H
#define __DRIVERS_BCH_BCH_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

//#include <ttos.h>
#include <fs/fs.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define MAX_OPENCNT (255) /* Limit of uint8_t */

/****************************************************************************
 * Public Types
 ****************************************************************************/

struct bchlib_s {
    struct inode *inode;    /* I-node of the block driver */
    uint32_t      sectsize; /* The size of one sector on the device */
    size_t        nsectors; /* Number of sectors supported by the device */
    size_t        sector;   /* The current sector in the buffer */
    MUTEX_ID      lock;     /* For atomic accesses to this structure */
    uint8_t       refs;     /* Number of references */
    bool          dirty;    /* true: Data has been written to the buffer */
    bool          readonly; /* true: Only read operations are supported */
    bool          unlinked; /* true: The driver has been unlinked */
    uint8_t      *buffer;   /* One sector buffer */

#if defined(CONFIG_BCH_ENCRYPTION)
    uint8_t key[CONFIG_BCH_ENCRYPTION_KEY_SIZE]; /* Encryption key */
#endif
};


/****************************************************************************
 * Public Data
 ****************************************************************************/

#undef EXTERN
#if defined(__cplusplus)
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

EXTERN const struct file_operations g_bch_fops;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

EXTERN int bchlib_flushsector (struct bchlib_s *bch);
EXTERN int bchlib_readsector (struct bchlib_s *bch, size_t sector);

/****************************************************************************
 * Name: devcrypto_register
 *
 * Description:
 *   Register /dev/crypto
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

void devcrypto_register (void);

/****************************************************************************
 * Name: devzero_register
 *
 * Description:
 *   Register /dev/zero
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

void devzero_register (void);

/****************************************************************************
 * Name: bchdev_register
 *
 * Description:
 *   Setup so that it exports the block driver referenced by 'blkdev' as a
 *   character device 'chardev'
 *
 ****************************************************************************/

int bchdev_register (const char *blkdev, const char *chardev, bool readonly);

/****************************************************************************
 * Name: bchdev_unregister
 *
 * Description:
 *   Unregister character driver access to a block device that was created
 *   by a previous call to bchdev_register().
 *
 ****************************************************************************/

int bchdev_unregister (const char *chardev);

/* Low level, direct access. NOTE: low-level access and character driver
 * access are incompatible. One and only one access method should be
 * implemented.
 */

/****************************************************************************
 * Name: bchlib_setup
 *
 * Description:
 *   Setup so that the block driver referenced by 'blkdev' can be accessed
 *   similar to a character device.
 *
 ****************************************************************************/

int bchlib_setup (const char *blkdev, bool readonly, void **handle);

/****************************************************************************
 * Name: bchlib_teardown
 *
 * Description:
 *   Setup so that the block driver referenced by 'blkdev' can be accessed
 *   similar to a character device.
 *
 ****************************************************************************/

int bchlib_teardown (void *handle);

/****************************************************************************
 * Name: bchlib_read
 *
 * Description:
 *   Read from the block device set-up by bchlib_setup as if it were a
 *   character device.
 *
 ****************************************************************************/

ssize_t bchlib_read (void *handle, char *buffer, size_t offset, size_t len);

/****************************************************************************
 * Name: bchlib_write
 *
 * Description:
 *   Write to the block device set-up by bchlib_setup as if it were a
 *   character device.
 *
 ****************************************************************************/

ssize_t bchlib_write (void *handle, const char *buffer, size_t offset,
                      size_t len);

#undef EXTERN
#if defined(__cplusplus)
}
#endif

#endif /* __DRIVERS_BCH_BCH_H */
