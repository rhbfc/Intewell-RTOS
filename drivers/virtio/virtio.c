/*-
 * Copyright (c) 2011, Bryan Venteicher <bryanv@FreeBSD.org>
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

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

#include "virtio_helper.h"
#include <stddef.h>

static const char *virtio_feature_name (unsigned long feature,
                                        const struct virtio_feature_desc *);

/*
 * TODO :
 * This structure may change depending on the types of devices we support.
 */
static const struct virtio_ident {
    unsigned short devid;
    const char    *name;
} virtio_ident_table[] = {
    {    VIRTIO_ID_NETWORK,         "Network"},
    {      VIRTIO_ID_BLOCK,           "Block"},
    {    VIRTIO_ID_CONSOLE,         "Console"},
    {    VIRTIO_ID_ENTROPY,         "Entropy"},
    {    VIRTIO_ID_BALLOON,         "Balloon"},
    {   VIRTIO_ID_IOMEMORY,        "IOMemory"},
    {       VIRTIO_ID_SCSI,            "SCSI"},
    {         VIRTIO_ID_9P,    "9P Transport"},
    {        VIRTIO_ID_GPU,             "GPU"},
    {      VIRTIO_ID_INPUT,           "Input"},
    {      VIRTIO_ID_VSOCK, "Vsock Transport"},
    {     VIRTIO_ID_CRYPTO,          "Crypto"},
    {      VIRTIO_ID_IOMMU,           "IOMMU"},
    {        VIRTIO_ID_MEM,          "Memory"},
    {      VIRTIO_ID_SOUND,           "Sound"},
    {         VIRTIO_ID_FS,     "File System"},
    {       VIRTIO_ID_PMEM,            "Pmem"},
    {       VIRTIO_ID_RPMB,            "RPMB"},
    {       VIRTIO_ID_SCMI,            "SCMI"},
    {VIRTIO_ID_I2C_ADAPTER,     "I2C Adapter"},
    {         VIRTIO_ID_BT,       "Bluetooth"},
    {       VIRTIO_ID_GPIO,            "GPIO"},
    {                    0,              NULL}
};

/* Device independent features. */
static const struct virtio_feature_desc virtio_common_feature_desc[] = {
    {   VIRTIO_F_NOTIFY_ON_EMPTY, "NotifyOnEmpty"},
    {VIRTIO_RING_F_INDIRECT_DESC,  "RingIndirect"},
    {    VIRTIO_RING_F_EVENT_IDX,      "EventIdx"},
    {       VIRTIO_F_BAD_FEATURE,    "BadFeature"},

    {                          0,            NULL}
};

const char *virtio_dev_name (unsigned short devid)
{
    const struct virtio_ident *ident;

    for (ident = virtio_ident_table; ident->name; ident++)
    {
        if (ident->devid == devid)
            return ident->name;
    }

    return NULL;
}

static const char *virtio_feature_name (unsigned long                     val,
                                        const struct virtio_feature_desc *desc)
{
    int                               i, j;
    const struct virtio_feature_desc *descs[2]
        = { desc, virtio_common_feature_desc };

    for (i = 0; i < 2; i++)
    {
        if (!descs[i])
            continue;

        for (j = 0; descs[i][j].vfd_val != 0; j++)
        {
            if (val == descs[i][j].vfd_val)
                return descs[i][j].vfd_str;
        }
    }

    return NULL;
}

void virtio_describe (struct virtio_device *dev, const char *msg,
                      uint32_t features, struct virtio_feature_desc *desc)
{
    (void)dev;
    (void)msg;
    (void)features;

    /* TODO: Not used currently - keeping it for future use*/
    virtio_feature_name (0, desc);
}
