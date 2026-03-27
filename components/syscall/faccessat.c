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
#include <fcntl.h>

/**
 * @brief 系统调用实现：检查文件访问权限。
 *
 * 该函数实现了一个系统调用，用于检查进程是否有权限访问指定文件。
 * 支持相对于目录文件描述符的路径。
 *
 * @param[in] dirfd 目录文件描述符：
 *                - AT_FDCWD: 使用当前工作目录
 *                - 其他值: 相对于该目录描述符
 * @param[in] pathname 要检查的文件路径
 * @param[in] mode 要检查的访问模式：
 *                - F_OK: 检查文件是否存在
 *                - R_OK: 检查读权限
 *                - W_OK: 检查写权限
 *                - X_OK: 检查执行权限
 * @param[in] flags 控制标志：
 *                - AT_EACCESS: 使用有效用户ID检查
 *                - AT_SYMLINK_NOFOLLOW: 不跟随符号链接
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 有指定的访问权限。
 * @retval -EACCES 没有指定的访问权限。
 * @retval -ENOENT 文件不存在。
 * @retval -ENOTDIR 路径组件不是目录。
 * @retval -EROFS 写访问请求用于只读文件系统。
 * @retval -EFAULT pathname指向无效内存。
 * @retval -EINVAL 无效参数。
 *
 * @note 1. 权限检查使用进程的真实用户ID，除非指定AT_EACCESS。
 *       2. 对于符号链接，默认跟随链接检查目标文件。
 *       3. 写权限检查会考虑文件系统是否只读。
 *       4. 执行权限要求文件存在且有执行位设置。
 */
DEFINE_SYSCALL (faccessat, (int dfd, const char __user *filename, int mode))
{
    if(dfd == AT_FDCWD && !user_string_access_ok(filename))
    {
        return -EFAULT;
    }
    char *fullpath = process_getfullpath (dfd, filename);
    int   ret      = access (fullpath, mode);
    free (fullpath);
    return ret;
}
