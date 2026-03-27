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

/**
 * @brief 复制文件描述符
 *
 * @param[in] oldfd 旧的文件描述符
 * @param[in] newfd 新的文件描述符
 * @param[in] flags 复制标志
 * @return 成功时返回新的文件描述符，失败时返回 -1 并设置 errno
 * @retval -EBADF: 文件描述符非法
 * @retval -EINTR: 调用被信号中断
 * @retval -EINVAL: flags包含无效值或者oldfd等于newfd
 * @retval -EMFILE: 文件描述符到达上限
 */
DEFINE_SYSCALL (dup3, (unsigned int oldfd, unsigned int newfd, int flags))
{
    int ret;
    ret = vfs_dup2(oldfd, newfd);

    if(ret < 0)
    {
        return ret;
    }

    if(O_CLOEXEC & flags)
    {
        ret = vfs_fcntl (newfd, F_SETFD, FD_CLOEXEC);
    }
    return ret;
}
