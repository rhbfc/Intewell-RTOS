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
 * @brief 系统调用实现：删除文件。
 *
 * 该函数实现了一个系统调用，用于删除一个文件名和可能的文件。
 * 通过调用unlinkat实现，使用AT_FDCWD作为基准目录。
 *
 * @param[in] pathname 要删除的文件路径
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功删除文件。
 * @retval -EACCES 没有写入/搜索权限。
 * @retval -EBUSY 文件正在被使用。
 * @retval -EFAULT pathname指向无效内存。
 * @retval -EIO I/O错误。
 * @retval -EISDIR pathname指向目录。
 * @retval -ELOOP 解析pathname时遇到太多符号链接。
 * @retval -ENAMETOOLONG pathname太长。
 * @retval -ENOENT pathname不存在。
 * @retval -EPERM 文件系统不允许删除。
 * @retval -EROFS 文件在只读文件系统上。
 *
 * @note 1. 如果是最后一个链接，文件会被删除。
 *       2. 如果文件仍在使用，要等到最后一个引用关闭。
 *       3. 不能用于删除目录，应使用rmdir。
 *       4. 成功后文件不能被恢复。
 */
DEFINE_SYSCALL (unlink, (const char __user *pathname))
{
    return SYSCALL_FUNC (unlinkat) (AT_FDCWD, pathname, 0);
}
