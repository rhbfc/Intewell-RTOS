/****************************************************************************
 * include/fs/kstat.h
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

#ifndef __KSTAT_H__
#define __KSTAT_H__
#ifdef __arm__
struct kstat
{
    dev_t st_dev;
    int __st_dev_padding;
    long __st_ino_truncated;
    mode_t st_mode;
    nlink_t st_nlink;
    uid_t st_uid;
    gid_t st_gid;
    dev_t st_rdev;
    int __st_rdev_padding;
    off_t st_size;
    blksize_t st_blksize;
    blkcnt_t st_blocks;
    long st_atime_sec;
    long st_atime_nsec;
    long st_mtime_sec;
    long st_mtime_nsec;
    long st_ctime_sec;
    long st_ctime_nsec;
    ino_t st_ino;
};
#elif defined(__aarch64__)
struct kstat
{
    dev_t st_dev;
    ino_t st_ino;
    mode_t st_mode;
    nlink_t st_nlink;
    uid_t st_uid;
    gid_t st_gid;
    dev_t st_rdev;
    unsigned long __pad;
    off_t st_size;
    blksize_t st_blksize;
    int __pad2;
    blkcnt_t st_blocks;
    long st_atime_sec;
    long st_atime_nsec;
    long st_mtime_sec;
    long st_mtime_nsec;
    long st_ctime_sec;
    long st_ctime_nsec;
    unsigned __reserved[2];
};
#elif defined(__x86_64__)
struct kstat
{
    dev_t st_dev;
    ino_t st_ino;
    nlink_t st_nlink;

    mode_t st_mode;
    uid_t st_uid;
    gid_t st_gid;
    unsigned int __pad0;
    dev_t st_rdev;
    off_t st_size;
    blksize_t st_blksize;
    blkcnt_t st_blocks;

    long st_atime_sec;
    long st_atime_nsec;
    long st_mtime_sec;
    long st_mtime_nsec;
    long st_ctime_sec;
    long st_ctime_nsec;
    long __reserved[3];
};
#endif
#endif /* __KSTAT_H__ */