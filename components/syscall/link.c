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

#ifdef CONFIG_PSEUDOFS_SOFTLINKS
#include "syscall_internal.h"
#include <fs/fs.h>
#include <errno.h>
#include <uaccess.h>
#include <unistd.h>

/**
 * @brief 系统调用实现：创建硬链接。
 *
 * 该函数实现了一个系统调用，用于创建一个文件的硬链接。
 *
 * @param[in] oldpath 现有文件的路径
 * @param[in] newpath 新链接的路径
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功创建硬链接。
 * @retval -EACCES 没有访问权限。
 * @retval -EEXIST newpath已存在。
 * @retval -EMLINK 文件的硬链接数达到上限。
 * @retval -ENOSPC 设备空间不足。
 *
 * @note 1. 硬链接与原文件共享inode。
 *       2. 只能对普通文件创建硬链接。
 *       3. 不能跨文件系统创建硬链接。
 *       4. 删除任一链接不影响其他链接。
 */
DEFINE_SYSCALL (linkat, (int olddfd, const char __user *oldname, int newdfd,
                          const char __user *newname, int flags));

DEFINE_SYSCALL (link, (const char __user *old, const char __user *new))
{
    return SYSCALL_FUNC(linkat) (AT_FDCWD, old, AT_FDCWD, new, 0);
}

#endif /* CONFIG_PSEUDOFS_SOFTLINKS */