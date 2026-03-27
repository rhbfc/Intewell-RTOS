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
#include <ttosProcess.h>
#include <errno.h>
#include <uaccess.h>
#include <sys/types.h>

/**
 * @brief 系统调用实现：通过相对路径改变文件所有者。
 *
 * 该函数实现了一个系统调用，用于修改指定路径文件的所有者和组。
 * 路径可以相对于指定的目录文件描述符。
 *
 * @param[in] dirfd 目录文件描述符：
 *                - AT_FDCWD: 使用当前工作目录
 *                - 其他值: 相对于该目录描述符
 * @param[in] pathname 文件路径
 * @param[in] owner 新的用户ID：
 *                - -1: 保持不变
 *                - 其他: 新的用户ID
 * @param[in] group 新的组ID：
 *                - -1: 保持不变
 *                - 其他: 新的组ID
 * @param[in] flags 控制标志：
 *                - AT_SYMLINK_NOFOLLOW: 不跟随符号链接
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功修改所有者。
 * @retval -EBADF dirfd不是有效的文件描述符。
 * @retval -ENOENT 文件不存在。
 * @retval -ENOTDIR 路径组件不是目录。
 * @retval -EPERM 进程无权修改所有者。
 * @retval -EROFS 文件系统是只读的。
 * @retval -EFAULT pathname指向无效内存。
 * @retval -EINVAL 无效的用户ID或组ID。
 *
 * @note 1. 只有特权进程可以修改文件所有者。
 *       2. 非特权进程只能将文件的组改为自己所属的组。
 *       3. 修改所有者会清除SUID和SGID位。
 *       4. 默认会跟随符号链接。
 */
DEFINE_SYSCALL (fchownat, (int dfd, const char __user *filename, uid_t user,
                           gid_t group, int flag))
{
    int   ret;
    char *fullpath;
    if (AT_EMPTY_PATH & flag)
    {
        if (dfd < 0)
        {
            return -EBADF;
        }
        return vfs_fchown (dfd, user, group);
    }
    fullpath = process_getfullpath (dfd, filename);
    if (fullpath == NULL)
    {
        return -ENOMEM;
    }
    if (AT_SYMLINK_NOFOLLOW & flag)
    {
        ret = vfs_lchown (fullpath, user, group);
    }
    else
    {
        ret = vfs_chown (fullpath, user, group);
    }
    free (fullpath);

    return ret;
}
