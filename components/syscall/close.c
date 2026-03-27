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
#include <errno.h>
#include <fs/fs.h>

/**
 * @brief 系统调用实现：关闭文件描述符。
 *
 * 该函数实现了一个系统调用，用于关闭文件描述符。
 *
 * @param[in] fd 文件描述符。
 * @return 成功时返回 0，失败时返回负值错误码。
 * @retval -EBADF fd 不是有效的打开文件描述符。
 * @retval -EINTR close() 调用被信号中断。
 * @retval -EIO 发生 I/O 错误。
 */
DEFINE_SYSCALL (close, (int fd))
{
    int ret;
    ret = vfs_close (fd);
    if (ret < 0)
    {
        errno = (-ret);
    }
    return ret;
}
