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
#include <fcntl.h>
#include <fs/fs.h>
#include <ttosProcess.h>
#include <uaccess.h>

/**
 * @brief 系统调用实现：相对路径打开文件。
 *
 * 该函数实现了一个系统调用，用于使用相对路径打开或创建文件。
 *
 * @param[in] dirfd 目录文件描述符
 * @param[in] pathname 相对路径
 * @param[in] flags 打开标志：
 *                  - O_RDONLY：只读
 *                  - O_WRONLY：只写
 *                  - O_RDWR：读写
 *                  - O_CREAT：不存在则创建
 *                  - O_EXCL：独占访问
 *                  - O_TRUNC：清空文件
 *                  - O_APPEND：追加写入
 * @param[in] mode 创建模式（仅用于O_CREAT）
 * @return 成功时返回文件描述符，失败时返回负值错误码。
 * @retval >=0 成功，返回文件描述符。
 * @retval -EACCES 没有访问权限。
 * @retval -EBADF dirfd无效。
 * @retval -EEXIST 文件已存在。
 * @retval -EMFILE 打开文件过多。
 *
 * @note 1. 路径相对于dirfd。
 *       2. dirfd为AT_FDCWD时使用当前目录。
 *       3. flags可以组合使用。
 *       4. mode指定新文件权限。
 */
DEFINE_SYSCALL(openat, (int dfd, const char __user *filename, int flags, mode_t mode))
{
    if (dfd == AT_FDCWD && filename != NULL && filename[0] == 0)
    {
        return -ENOENT;
    }
    char *fullpath = process_getfullpath(dfd, filename);
    if (fullpath == NULL)
    {
        return -EINVAL;
    }

    /* 处理试图以文件夹的方式打开文件 */
    if ((flags & O_DIRECTORY))
    {
        struct stat st;
        if (vfs_stat(fullpath, &st, 0) < 0)
        {
            free(fullpath);
            return -ENOENT;
        }
        if (!S_ISDIR(st.st_mode))
        {
            free(fullpath);
            return -ENOTDIR; // 不是目录
        }
    }

    if (!(flags & O_CREAT) && !(flags & O_DIRECTORY))
    {
        struct stat st;
        if (vfs_stat(fullpath, &st, 0) < 0)
        {
            free(fullpath);
            return -ENOENT;
        }
        if (S_ISDIR(st.st_mode))
        {
            flags |= O_DIRECTORY;
        }
    }
    int fd = vfs_open(fullpath, flags, mode);
    free(fullpath);
    return fd;
}
