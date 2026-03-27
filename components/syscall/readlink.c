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

/**
 * @brief 系统调用实现：读取符号链接。
 *
 * 该函数实现了一个系统调用，用于读取符号链接的目标路径。
 *
 * @param[in] path 符号链接路径
 * @param[out] buf 存放目标路径的缓冲区
 * @param[in] bufsiz 缓冲区大小
 * @return 成功时返回读取的字节数，失败时返回负值错误码。
 * @retval >0 成功读取的字节数。
 * @retval -EINVAL 参数无效。
 * @retval -ENOENT 文件不存在。
 * @retval -ENOTDIR 路径组件非目录。
 *
 * @note 1. 不追踪链接。
 *       2. 返回原始路径。
 *       3. 长度受限。
 *       4. 权限检查。
 */
DEFINE_SYSCALL (readlink,
                (const char __user *path, char __user *buf, int bufsiz))
{
    return SYSCALL_FUNC (readlinkat) (AT_FDCWD, path, buf, bufsiz);
}