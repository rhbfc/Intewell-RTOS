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
#include <errno.h>
#include <fs/fs.h>
#include <ttosProcess.h>

DEFINE_SYSCALL (symlinkat, (const char __user *oldname, int newdfd,
                             const char __user *newname));

/**
 * @brief 系统调用实现：创建符号链接。
 *
 * 该函数实现了一个系统调用，用于创建一个指向目标文件的符号链接。
 *
 * @param[in] old 目标文件的路径名
 * @param[in] new 要创建的符号链接的路径名
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功创建符号链接。
 * @retval -EACCES 没有写入权限。
 * @retval -EEXIST 目标文件已存在。
 * @retval -ENAMETOOLONG 路径名过长。
 * @retval -ENOSPC 设备空间不足。
 *
 * @note 1. 目标文件不需要存在。
 *       2. 需要目标目录的写入权限。
 *       3. 符号链接的目标路径长度有限制。
 *       4. 该函数是symlinkat的简化版本。
 */
DEFINE_SYSCALL (symlink, (const char __user *old, const char __user *new))
{
    return SYSCALL_FUNC(symlinkat)(old, AT_FDCWD, new);
}

#endif /* CONFIG_PSEUDOFS_SOFTLINKS */