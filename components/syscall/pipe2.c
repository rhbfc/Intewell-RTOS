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
#include <fcntl.h>

/**
 * @brief 系统调用实现：创建管道（带标志）。
 *
 * 该函数实现了一个系统调用，用于创建一个带有指定标志的单向数据通道。
 *
 * @param[out] fildes 返回两个文件描述符：
 *                    - fildes[0]：读端
 *                    - fildes[1]：写端
 * @param[in] flags 管道标志：
 *                  - O_NONBLOCK：非阻塞模式
 *                  - O_CLOEXEC：执行时关闭
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功创建管道。
 * @retval -EFAULT 内存访问错误。
 * @retval -EINVAL 标志无效。
 * @retval -EMFILE 打开文件过多。
 *
 * @note 1. 支持额外标志。
 *       2. 单向数据流。
 *       3. FIFO顺序。
 *       4. 进程间通信。
 */
DEFINE_SYSCALL (pipe2, (int __user *fildes, int flags))
{
    int fd[2];

    int ret = vfs_pipe2 (fd, flags);

    copy_to_user (fildes, fd, sizeof (fd));

    return ret;
}
