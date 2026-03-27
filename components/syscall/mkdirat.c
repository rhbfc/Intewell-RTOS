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
 * @brief 系统调用实现：相对路径创建目录。
 *
 * 该函数实现了一个系统调用，用于使用相对路径创建新目录。
 *
 * @param[in] dirfd 目录文件描述符
 * @param[in] pathname 相对路径
 * @param[in] mode 目录权限模式
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功创建目录。
 * @retval -EACCES 没有写入权限。
 * @retval -EBADF dirfd无效。
 * @retval -EEXIST 目录已存在。
 * @retval -ENOENT 父目录不存在。
 *
 * @note 1. 路径相对于dirfd。
 *       2. dirfd为AT_FDCWD时使用当前目录。
 *       3. mode受umask影响。
 *       4. 创建"."和".."。
 */
DEFINE_SYSCALL (mkdirat, (int dfd, const char __user *pathname, mode_t mode))
{
    char *fullpath = NULL;
    int   fd = -1;
    if(pathname == NULL)
    {
        return -ENOENT;
    }
    if(strlen_user(pathname) >= PATH_MAX)
    {
        return -ENAMETOOLONG;
    }

    fullpath = process_getfullpath (dfd, pathname);
    fd       = vfs_mkdir (fullpath, mode);
    free (fullpath);
    return fd;
}
