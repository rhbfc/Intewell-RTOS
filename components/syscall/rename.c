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
 * @brief 系统调用实现：重命名文件或目录。
 *
 * 该函数实现了一个系统调用，用于重命名文件或目录。
 *
 * @param[in] oldpath 原路径名
 * @param[in] newpath 新路径名
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功。
 * @retval -EACCES 权限不足。
 * @retval -ENOENT 文件不存在。
 * @retval -EINVAL 参数无效。
 *
 * @note 1. 支持跨目录。
 *       2. 原子操作。
 *       3. 目标存在时覆盖。
 *       4. 不支持跨文件系统。
 */
DEFINE_SYSCALL (rename,
                (const char __user *oldname, const char __user *newname))
{
    return SYSCALL_FUNC (renameat) (AT_FDCWD, oldname, AT_FDCWD, newname);
}
