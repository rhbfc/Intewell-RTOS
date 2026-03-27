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
#include <uaccess.h>

/**
 * @brief 系统调用实现：检查文件访问权限。
 *
 * 该函数实现了一个系统调用，用于检查文件访问权限。
 *
 * @param[in] filename 文件名。
 * @param[in] mode 访问模式。可以是以下值的组合：
 *          - R_OK：检查读权限。
 *          - W_OK：检查写权限。
 *          - X_OK：检查执行权限。
 *          - F_OK：检查文件是否存在。
 * @return 成功时返回 0，失败时返回负值错误码。
 * @retval: -EACCES 请求的访问被拒绝，或者路径前缀中的一个目录没有搜索权限。
 * @retval: -ELOOP 解析路径时遇到太多的符号链接。
 * @retval: -ENAMETOOLONG 路径名太长。
 * @retval: -ENOENT 路径名的一个组件不存在，或者是一个悬空的符号链接。
 * @retval: -ENOTDIR 路径名中的一个组件被用作目录，但实际上不是目录。
 * @retval: -EROFS 请求对只读文件系统上的文件进行写操作。
 * @retval: -EFAULT filename 指向了不可访问的地址空间。
 * @retval: -EINVAL mode 错误。
 * @retval: -EIO I/O 错误。
 * @retval: -ENOMEM 内存不足。
 * @retval: -ETXTBSY 请求对正在执行的可执行文件进行写操作。
 */
DEFINE_SYSCALL (access, (const char __user *filename, int mode))
{
    return SYSCALL_FUNC (faccessat) (AT_FDCWD, filename, mode);
}
