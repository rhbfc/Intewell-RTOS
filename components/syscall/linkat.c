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

#ifdef CONFIG_PSEUDOFS_SOFTLINKS
#include "syscall_internal.h"
#include <ttosProcess.h>
#include <errno.h>
#include <uaccess.h>
#include <fcntl.h>

/**
 * @brief 系统调用实现：相对路径创建硬链接。
 *
 * 该函数实现了一个系统调用，用于使用相对路径创建文件的硬链接。
 *
 * @param[in] olddirfd 原文件所在目录的文件描述符
 * @param[in] oldpath 原文件的相对路径
 * @param[in] newdirfd 新链接所在目录的文件描述符
 * @param[in] newpath 新链接的相对路径
 * @param[in] flags 控制链接行为的标志
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功创建硬链接。
 * @retval -EACCES 没有访问权限。
 * @retval -EEXIST newpath已存在。
 * @retval -EMLINK 文件的硬链接数达到上限。
 * @retval -ENOSPC 设备空间不足。
 *
 * @note 1. AT_EMPTY_PATH允许oldpath为空。
 *       2. AT_SYMLINK_FOLLOW跟随符号链接。
 *       3. 路径相对于dirfd指定的目录。
 *       4. dirfd为AT_FDCWD时使用当前目录。
 */
DEFINE_SYSCALL (linkat, (int olddirfd, const char __user *oldpath, int newdirfd,
                          const char __user *newpath, int flags))
{
    int ret;
    char *fullpath_old = process_getfullpath (olddirfd, oldpath);
    if (!fullpath_old)
    {
        return -ENOMEM;
    }
    if(AT_SYMLINK_FOLLOW & flags)
    {
        char *linkpath_old = malloc (PATH_MAX);
        if (linkpath_old == NULL)
        {
            return -ENOMEM;
        }
        realpath(fullpath_old, linkpath_old);
        free(fullpath_old);
        fullpath_old = linkpath_old;
    }
    char *fullpath_new = process_getfullpath (newdirfd, newpath);
    if (!fullpath_new)
    {
        free(fullpath_old);
        return -ENOMEM;
    }
    ret = vfs_link (fullpath_old, fullpath_new);

    free(fullpath_old);
    free(fullpath_new);

    if (ret < 0)
    {
        errno = -ret;
    }
    return ret;
}

#endif /* CONFIG_PSEUDOFS_SOFTLINKS */