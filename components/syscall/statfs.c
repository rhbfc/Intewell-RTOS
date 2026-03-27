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
#include <errno.h>
#include <fs/fs.h>
#include <uaccess.h>
#include <ttosProcess.h>

/**
 * @brief 系统调用实现：获取文件系统信息。
 *
 * 该函数实现了一个系统调用，用于获取指定路径所在文件系统的信息。
 *
 * @param[in] path 要获取信息的文件系统上的路径
 * @param[out] buf 用于存储文件系统信息的结构体：
 *                 - f_type：文件系统类型
 *                 - f_bsize：最优传输块大小
 *                 - f_blocks：文件系统数据块总数
 *                 - f_bfree：空闲块数
 *                 - f_bavail：非超级用户可用块数
 *                 - f_files：文件节点总数
 *                 - f_ffree：空闲节点数
 *                 - f_fsid：文件系统标识
 *                 - f_namelen：文件名长度限制
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功获取信息。
 * @retval -EINVAL 无效参数。
 * @retval -EFAULT buf指向无效内存。
 * @retval -EACCES 权限不足。
 * @retval -ENAMETOOLONG 路径名过长。
 * @retval -ENOENT 路径不存在。
 * @retval -EIO I/O错误。
 *
 * @note 1. 路径可以是文件系统上的任意文件。
 *       2. 某些字段可能不被所有文件系统支持。
 *       3. 可用于检查磁盘空间。
 *       4. 需要路径上的搜索权限。
 */
DEFINE_SYSCALL (statfs, (const char __user *path, struct statfs __user *buf))
{
    int ret = 0;
    struct statfs kbuf;
    if( (path != NULL) && (strlen_user(path) >= PATH_MAX) )
    {
        return -ENAMETOOLONG;   /* 文件路径太长 */
    }

    if (!user_access_check (buf, sizeof (struct statfs), UACCESS_W))
    {
        return -EINVAL;
    }

    char *fullpath = process_getfullpath (AT_FDCWD, path);
    if (!fullpath)
    {
        return -EINVAL;
    }
    memset(&kbuf, 0, sizeof(kbuf));
    ret = vfs_statfs(fullpath, &kbuf);
    free(fullpath);

    if(ret < 0)
    {
        return ret;
    }
    ret = copy_to_user (buf, &kbuf, sizeof (kbuf));

    return ret;
}