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
#include <sys/stat.h>

/**
 * @brief 系统调用实现：创建特殊文件。
 *
 * 该函数实现了一个系统调用，用于创建特殊文件（设备文件、管道等）。
 *
 * @param[in] pathname 文件路径
 * @param[in] mode 文件类型和权限：
 *                 - S_IFREG：普通文件
 *                 - S_IFCHR：字符设备
 *                 - S_IFBLK：块设备
 *                 - S_IFIFO：FIFO
 *                 - S_IFSOCK：套接字
 * @param[in] dev 设备号（仅用于设备文件）
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功创建特殊文件。
 * @retval -EACCES 没有写入权限。
 * @retval -EEXIST 文件已存在。
 * @retval -EPERM 权限不足。
 * @retval -EROFS 只读文件系统。
 *
 * @note 1. 通常需要特权权限。
 *       2. mode包含文件类型和权限。
 *       3. dev仅用于设备文件。
 *       4. 不能在网络文件系统上使用。
 */
DEFINE_SYSCALL (mknod, (const char __user *filename, mode_t mode, unsigned dev))
{
    return SYSCALL_FUNC (mknodat) (AT_FDCWD, filename, mode, dev);
}