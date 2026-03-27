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
#include <dirent.h>

/**
 * @brief 系统调用实现：获取目录项。
 *
 * 该函数实现了一个系统调用，用于读取目录的内容，返回一系列
 * 目录项结构。
 *
 * @param[in] fd 目录的文件描述符
 * @param[out] dirp 存储目录项的缓冲区
 * @param[in] count 缓冲区大小
 * @return 成功时返回读取的字节数，失败时返回负值错误码。
 * @retval >0 成功，返回读取的字节数。
 * @retval 0 到达目录末尾。
 * @retval -EBADF fd不是有效的文件描述符。
 * @retval -ENOTDIR fd不是目录。
 * @retval -EINVAL 参数无效。
 * @retval -EFAULT dirp指向无效内存。
 *
 * @note 1. 每个目录项包含inode号、偏移和名字。
 *       2. 返回的目录项是紧密打包的。
 *       3. 可以多次调用读取所有项。
 *       4. 目录项的顺序是文件系统相关的。
 */
DEFINE_SYSCALL (getdents, (int fd, struct dirent __user *dirent,
                           unsigned int count))
{
    return SYSCALL_FUNC (read) (fd, (void *)dirent, count);
}

__alias_syscall (getdents, getdents64);
