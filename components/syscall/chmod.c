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
 * @brief 系统调用实现：修改文件权限。
 *
 * 该函数实现了一个系统调用，用于修改文件权限。
 *
 * @param[in] filename 文件名。
 * @param[in] mode 新的权限, 可以是以下值的组合：
 *          - S_IRUSR：用户读权限。
 *          - S_IWUSR：用户写权限。
 *          - S_IXUSR：用户执行权限。
 *          - S_IRGRP：组读权限。
 *          - S_IWGRP：组写权限。
 *          - S_IXGRP：组执行权限。
 *          - S_IROTH：其他用户读权限。
 *          - S_IWOTH：其他用户写权限。
 *          - S_IXOTH：其他用户执行权限。
 *          
 * @return 成功时返回 0，失败时返回负值错误码。
 * @retval -EACCES 请求的访问被拒绝，或者路径前缀中的一个目录没有搜索权限。
 * @retval -ELOOP 解析路径时遇到太多的符号链接。
 * @retval -ENAMETOOLONG 路径名太长。
 * @retval -ENOENT 路径名的一个组件不存在，或者是一个悬空的符号链接。
 * @retval -ENOTDIR 路径名中的一个组件被用作目录，但实际上不是目录。
 * @retval -EROFS 请求对只读文件系统上的文件进行写操作。
 * @retval -EFAULT filename 指向了不可访问的地址空间。
 * @retval -EINVAL mode 错误。
 * @retval -EIO I/O 错误。
 * @retval -ENOMEM 内存不足。
 * @retval -EPERM 有效 UID 与文件所有者不匹配，且进程没有特权
 * @retval -EPERM 文件被标记为不可变或仅追加。
 * @retval -EROFS 文件位于只读文件系统上。
 * @retval -EINVAL flags 中指定了无效标志。
 */
DEFINE_SYSCALL (chmod, (const char __user *filename, mode_t mode))
{
    return SYSCALL_FUNC (fchmodat) (AT_FDCWD, filename, mode);
}
