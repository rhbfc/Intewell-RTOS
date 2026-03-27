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

#include "syscall_internal.h"
#include <assert.h>
#include <errno.h>
#include <fs/fs.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <ttosProcess.h>
#include <uaccess.h>

#include <fs/kstat.h>

/**
 * @brief 系统调用实现：获取文件状态。
 *
 * 该函数实现了一个系统调用，用于获取文件状态。
 *
 * @param[in] fd 文件描述符。
 * @param[out] statbuf 指向用户空间的文件状态结构体指针。
 * @return 成功时返回 0，失败时返回负值错误码。
 * @retval -EACCES: 请求的访问被拒绝，或者路径前缀中的一个目录没有搜索权限。
 * @retval -EBADF: 文件夹描述符无效。 
 * @retval -EFAULT: 错误地址
 * @retval -ELOOP: 解析路径时遇到太多的符号链接。
 * @retval -ENOMEM: 内存不足。
 */
DEFINE_SYSCALL (fstat, (int fd, struct stat __user *statbuf))
{
    int         ret;
    struct stat stbuf;
    pcb_t       pcb = ttosProcessSelf ();
    assert (pcb != NULL);
    struct kstat kstxat;

    if (!user_access_check (statbuf, sizeof (struct kstat), UACCESS_W))
    {
        return -EINVAL;
    }

    ret = vfs_fstat(fd, &stbuf);

    if (ret == 0)
    {
        kstxat = (struct kstat){ .st_dev        = stbuf.st_dev,
                                 .st_rdev       = stbuf.st_rdev,
                                 .st_ino        = stbuf.st_ino,
                                 .st_mode       = stbuf.st_mode,
                                 .st_nlink      = stbuf.st_nlink,
                                 .st_uid        = stbuf.st_uid,
                                 .st_gid        = stbuf.st_gid,
                                 .st_size       = stbuf.st_size,
                                 .st_blksize    = stbuf.st_blksize,
                                 .st_blocks     = stbuf.st_blocks,
                                 .st_atime_sec  = (long)stbuf.st_atim.tv_sec,
                                 .st_atime_nsec = (long)stbuf.st_atim.tv_nsec,
                                 .st_mtime_sec  = (long)stbuf.st_mtim.tv_sec,
                                 .st_mtime_nsec = (long)stbuf.st_mtim.tv_nsec,
                                 .st_ctime_sec  = (long)stbuf.st_ctim.tv_sec,
                                 .st_ctime_nsec = (long)stbuf.st_ctim.tv_nsec };

        copy_to_user (statbuf, &kstxat, sizeof (kstxat));
    }

    return ret;
}