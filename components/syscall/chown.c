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

/**
 * @brief 系统调用实现：更改文件的所有者和组。
 *
 * @param[in] filename 指向文件名的指针。
 * @param[in] user 设置为所有者的用户 ID。
 * @param[in] group 设置为组所有者的组 ID。
 *
 * @return 成功返回 0，失败返回负错误代码。
 * @retval -EACCES 路径前缀中的一个目录没有搜索权限。
 * @retval -EFAULT filename 指向了不可访问的地址空间。
 * @retval -ELOOP 解析路径时遇到太多的符号链接。
 * @retval -ENAMETOOLONG 路径名太长。
 * @retval -ENOENT 文件不存在。
 * @retval -ENOMEM 内核内存不足。
 * @retval -ENOTDIR 路径前缀中的一个组件不是目录。
 * @retval -EPERM 调用进程没有足够的权限。
 * @retval -EPERM 文件被标记为不可变或仅追加。
 * @retval -EROFS 文件位于只读文件系统上。
 * @retval -EIO I/O 错误。
 */
DEFINE_SYSCALL (chown, (const char __user *filename, uid_t user, gid_t group))
{
    return SYSCALL_FUNC (fchownat) (AT_FDCWD, filename, user, group, 0);
}

__alias_syscall (chown, chown32);
