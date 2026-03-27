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
#include <ttosProcess.h>
#include <uaccess.h>

/**
 * @brief 系统调用实现：更新文件的访问和修改时间戳。
 *
 * 该函数实现了一个系统调用，用于更新指定文件的访问时间（atime）
 * 和修改时间（mtime）。支持相对路径和绝对路径，可以精确到纳秒。
 *
 * @param[in] dfd 目录文件描述符：
 *                - AT_FDCWD: 使用当前工作目录
 *                - 其他值: 相对于该目录的文件描述符
 * @param[in] filename 文件路径：
 *                   - NULL: 操作dfd指向的文件
 *                   - 绝对路径: 忽略dfd
 *                   - 相对路径: 相对于dfd指定的目录
 * @param[in] utimes 时间数组，包含两个timespec结构：
 *                - utimes[0]: 访问时间（atime）
 *                - utimes[1]: 修改时间（mtime）
 *                - NULL: 都设置为当前时间
 * @param[in] flags 控制标志：
 *                - 0: 正常操作
 *                - AT_SYMLINK_NOFOLLOW: 不跟随符号链接
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 操作成功。
 * @retval -EFAULT filename或utimes指向无效内存。
 * @retval -EINVAL flags无效。
 * @retval -EACCES 没有权限修改文件时间戳。
 * @retval -ENOENT 文件不存在。
 * @retval -ENOTDIR dfd指定的不是目录。
 *
 * @note 1. 如果utimes为NULL，两个时间戳都会被设置为当前时间。
 *       2. 如果utimes中的tv_nsec字段为UTIME_NOW，对应的时间戳
 *          会被设置为当前时间。
 *       3. 如果tv_nsec为UTIME_OMIT，对应的时间戳保持不变。
 */
DEFINE_SYSCALL (utimensat, (int dfd, const char __user *filename,
                            struct timespec __user *utimes, int flags))
{
    struct timespec kutime[2];
    int             ret;
    long            ktimes[4] = { 0, 0 };
    if (utimes != NULL)
    {
        ret = copy_from_user (&ktimes, utimes, sizeof (ktimes));
        if (ret < 0)
        {
            return ret;
        }
    }
    kutime[0].tv_sec  = ktimes[0];
    kutime[0].tv_nsec = ktimes[1];
    kutime[1].tv_sec  = ktimes[2];
    kutime[1].tv_nsec = ktimes[3];

    char *fullpath = process_getfullpath (dfd, filename);

    if (access (fullpath, W_OK) != 0)
    {
        ret = -ENOENT;
        goto failed;
    }

    ret = vfs_utimens (fullpath, utimes == NULL ? NULL : (const struct timespec *)&kutime);

failed:
    free (fullpath);

    return ret;
}
