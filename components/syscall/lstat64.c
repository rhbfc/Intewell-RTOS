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
#include <fs/fs.h>
#include <errno.h>
#include <uaccess.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <sys/sysmacros.h>
#include <ttosProcess.h>
#include <assert.h>
#include <fs/kstat.h>

/**
 * @brief 系统调用实现：获取符号链接状态。
 *
 * 该函数实现了一个系统调用，用于获取符号链接本身的状态信息。
 *
 * @param[in] pathname 符号链接的路径
 * @param[out] statbuf 用于存储状态信息的缓冲区
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功获取状态信息。
 * @retval -EACCES 没有搜索权限。
 * @retval -EFAULT statbuf指向无效内存。
 * @retval -ENAMETOOLONG 路径名过长。
 * @retval -ENOENT 路径不存在。
 *
 * @note 1. 不会跟随符号链接。
 *       2. 使用64位文件大小。
 *       3. 返回链接本身的信息。
 *       4. 需要搜索目录的权限。
 */
DEFINE_SYSCALL (lstat64,
                (const char __user *pathname, struct stat __user *statbuf))
{
    int         ret;
    struct stat stbuf;
    pcb_t       pcb = ttosProcessSelf ();
    assert (pcb != NULL);
    struct kstat kstxat;

    if ((pathname != NULL) && (strlen_user (pathname) >= PATH_MAX))
    {
        return -ENAMETOOLONG; /* 文件路径太长 */
    }

    if (!user_access_check (statbuf, sizeof (struct kstat), UACCESS_W))
    {
        return -EINVAL;
    }

    char *fullpath = process_getfullpath (AT_FDCWD, pathname);
    if (!fullpath)
    {
        return -1;
    }

    if (access (fullpath, F_OK) != 0)
    {
        free (fullpath);
        return -ENOENT;
    }

    ret = vfs_stat (fullpath, &stbuf, 0);

    free (fullpath);

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