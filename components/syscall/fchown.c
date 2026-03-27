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
#include <sys/types.h>

/**
 * @brief 系统调用实现：通过文件描述符改变文件所有者。
 *
 * 该函数实现了一个系统调用，用于修改由文件描述符引用的文件的所有者和组。
 *
 * @param[in] fd 文件描述符
 * @param[in] owner 新的用户ID：
 *                - -1: 保持不变
 *                - 其他: 新的用户ID
 * @param[in] group 新的组ID：
 *                - -1: 保持不变
 *                - 其他: 新的组ID
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功修改所有者。
 * @retval -EBADF fd不是有效的文件描述符。
 * @retval -EPERM 进程无权修改所有者。
 * @retval -EROFS 文件系统是只读的。
 * @retval -EINVAL 无效的用户ID或组ID。
 *
 * @note 1. 只有特权进程可以修改文件所有者。
 *       2. 非特权进程只能将文件的组改为自己所属的组。
 *       3. 修改所有者会清除SUID和SGID位。
 *       4. 符号链接的所有者不能被修改。
 */
DEFINE_SYSCALL (fchown, (int fd, uid_t owner, gid_t group))
{
    return vfs_fchown(fd, owner, group);
}

__alias_syscall(fchown, fchown32);