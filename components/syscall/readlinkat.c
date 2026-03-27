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
#include <fs/fs.h>
#include <errno.h>
#include <uaccess.h>
#include <unistd.h>
#include <ttosProcess.h>

/**
 * @brief 系统调用实现：相对路径读取符号链接。
 *
 * 该函数实现了一个系统调用，用于读取相对于给定目录的符号链接的目标路径。
 *
 * @param[in] dirfd 目录文件描述符：
 *                  - AT_FDCWD：使用当前工作目录
 *                  - 其他值：使用指定目录
 * @param[in] pathname 符号链接相对路径
 * @param[out] buf 存放目标路径的缓冲区
 * @param[in] bufsiz 缓冲区大小
 * @return 成功时返回读取的字节数，失败时返回负值错误码。
 * @retval >0 成功读取的字节数。
 * @retval -EBADF 无效的文件描述符。
 * @retval -EINVAL 参数无效。
 * @retval -ENOENT 文件不存在。
 *
 * @note 1. 支持相对路径。
 *       2. 不追踪链接。
 *       3. 长度受限。
 *       4. 权限检查。
 */
DEFINE_SYSCALL (readlinkat, (int dfd, const char __user *path,
                              char __user *buf, int bufsiz))

{
    int ret;
    if (!user_access_check (buf, bufsiz, UACCESS_W))
    {
        return -EINVAL;
    }

    char *fullpath = process_getfullpath (dfd, path);
    if (!fullpath)
    {
        return -ENOMEM;
    }

    if (access (fullpath, F_OK) != 0)
    {
        free (fullpath);
        return -ENOENT;
    }

    char * kbuf = malloc(bufsiz);
    if(kbuf == NULL)
    {
        return -ENOMEM;
    }
    ret = vfs_readlink(fullpath, kbuf, bufsiz);

    if(ret > 0)
    {
        copy_to_user(buf, kbuf, ret);
    }
    free (fullpath);

    return ret;
}