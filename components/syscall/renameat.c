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
 * @brief 系统调用实现：相对路径重命名文件或目录。
 *
 * 该函数实现了一个系统调用，用于相对于目录文件描述符重命名文件或目录。
 *
 * @param[in] olddirfd 原路径相对的目录文件描述符
 * @param[in] oldpath 原路径名
 * @param[in] newdirfd 新路径相对的目录文件描述符
 * @param[in] newpath 新路径名
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功。
 * @retval -EACCES 权限不足。
 * @retval -ENOENT 文件不存在。
 * @retval -EINVAL 参数无效。
 *
 * @note 1. 支持相对路径。
 *       2. 原子操作。
 *       3. 目标存在时覆盖。
 *       4. 不支持跨文件系统。
 */
DEFINE_SYSCALL (renameat, (int olddirfd, const char __user *oldpath, int newdirfd,
                           const char __user *newpath))
{
    int   ret;
    ret = strlen_user(oldpath);
    if(ret < 0)
    {
        if(ret == -EINVAL)
            ret = -EFAULT;  /* 保持与linux一致 */
        return ret;
    }

    ret = strlen_user(newpath);
    if(ret < 0)
    {
        return ret;
    }
    char *old_fullpath = process_getfullpath (olddirfd, oldpath);
    char *new_fullpath = process_getfullpath (newdirfd, newpath);
    ret                = vfs_rename (old_fullpath, new_fullpath);
    free (old_fullpath);
    free (new_fullpath);
    if (ret < 0)
    {
        errno = -ret;
    }
    return ret;
}
