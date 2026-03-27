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
#include <sys/stat.h>

/**
 * @brief 系统调用实现：通过相对路径改变文件权限。
 *
 * 该函数实现了一个系统调用，用于修改指定路径文件的权限模式。
 * 路径可以相对于指定的目录文件描述符。
 *
 * @param[in] dirfd 目录文件描述符：
 *                - AT_FDCWD: 使用当前工作目录
 *                - 其他值: 相对于该目录描述符
 * @param[in] pathname 文件路径
 * @param[in] mode 新的权限模式：
 *                - S_ISUID (04000) 设置用户ID
 *                - S_ISGID (02000) 设置组ID
 *                - S_ISVTX (01000) 粘着位
 *                - S_IRUSR (00400) 用户读权限
 *                - S_IWUSR (00200) 用户写权限
 *                - S_IXUSR (00100) 用户执行权限
 *                - S_IRGRP (00040) 组读权限
 *                - S_IWGRP (00020) 组写权限
 *                - S_IXGRP (00010) 组执行权限
 *                - S_IROTH (00004) 其他读权限
 *                - S_IWOTH (00002) 其他写权限
 *                - S_IXOTH (00001) 其他执行权限
 * @param[in] flags 控制标志：
 *                - AT_SYMLINK_NOFOLLOW: 不跟随符号链接
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功修改权限。
 * @retval -EBADF dirfd不是有效的文件描述符。
 * @retval -ENOENT 文件不存在。
 * @retval -ENOTDIR 路径组件不是目录。
 * @retval -EPERM 进程无权修改文件权限。
 * @retval -EROFS 文件系统是只读的。
 * @retval -EFAULT pathname指向无效内存。
 *
 * @note 1. 只有文件所有者或特权进程可以修改文件权限。
 *       2. 某些权限位可能被文件系统忽略。
 *       3. 最终权限会受umask影响。
 *       4. 默认会跟随符号链接。
 */
DEFINE_SYSCALL (fchmodat, (int dfd, const char __user *filename, mode_t mode))
{
    if (!user_string_access_ok (filename))
    {
        return -EFAULT;
    }
    if (mode < 0)
    {
        return -EINVAL;
    }
    char *fullpath = process_getfullpath (dfd, filename);
    if (fullpath == NULL)
    {
        return -ENOENT;
    }

    int ret = vfs_chmod (fullpath, mode);

    free (fullpath);

    return ret;
}