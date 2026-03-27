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
#include <errno.h>
#include <fs/fs.h>
#include <ttosProcess.h>

/**
 * @brief 系统调用实现：相对路径创建符号链接。
 *
 * 该函数实现了一个系统调用，用于创建一个指向目标文件的符号链接，
 * 支持相对于文件描述符的路径。
 *
 * @param[in] oldname 目标文件的路径名
 * @param[in] newdfd 新链接名相对的目录文件描述符：
 *                   - AT_FDCWD：相对于当前工作目录
 *                   - 其他值：相对于指定的目录文件描述符
 * @param[in] newname 要创建的符号链接的路径名
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功创建符号链接。
 * @retval -EACCES 没有写入权限。
 * @retval -EEXIST 目标文件已存在。
 * @retval -ENAMETOOLONG 路径名过长。
 * @retval -ENOENT 目标文件不存在。
 * @retval -ENOMEM 内存不足。
 * @retval -ENOSPC 设备空间不足。
 * @retval -EBADF 无效的目录文件描述符。
 *
 * @note 1. 目标文件必须存在。
 *       2. 需要目标目录的写入权限。
 *       3. 符号链接的目标路径长度有限制。
 *       4. 提供了比symlink更灵活的路径处理。
 */
DEFINE_SYSCALL (symlinkat, (const char __user *oldname, int newdfd,
                             const char __user *newname))
{
    int ret;
    struct stat stat_buf;

    char *fullpath_old = process_getfullpath (AT_FDCWD, oldname);
    if (!fullpath_old)
    {
        return -ENOMEM;
    }

    /* 检测文件是否存在 */
    if(stat(fullpath_old, &stat_buf) != 0)
    {
        return -ENOENT;
    }
    
    char *fullpath_new = process_getfullpath (newdfd, newname);
    if (!fullpath_new)
    {
        free(fullpath_old);
        return -ENOMEM;
    }
    ret = vfs_symlink (fullpath_old, fullpath_new);

    free(fullpath_old);
    free(fullpath_new);

    if (ret < 0)
    {
        errno = -ret;
    }
    return ret;
}

#endif /* CONFIG_PSEUDOFS_SOFTLINKS */