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
#include <sys/epoll.h>

/**
 * @brief 系统调用实现：创建epoll实例。
 *
 * 该函数实现了一个系统调用，用于创建一个新的epoll实例。
 * epoll是Linux的高效I/O多路复用机制。
 *
 * @param[in] size 提示内核期望监视的文件描述符数量（已废弃，但必须大于0）
 * @return 成功时返回新的epoll文件描述符，失败时返回负值错误码。
 * @retval >0 新创建的epoll文件描述符。
 * @retval -EINVAL size参数无效（小于等于0）。
 * @retval -ENOMEM 内存不足。
 * @retval -EMFILE 达到进程打开文件描述符上限。
 * @retval -ENFILE 达到系统打开文件描述符上限。
 *
 * @note 1. size参数在Linux 2.6.8之后被忽略，但必须大于0。
 *       2. 返回的文件描述符用于后续的epoll_ctl和epoll_wait调用。
 *       3. epoll实例在最后一个引用关闭时自动销毁。
 *       4. epoll比select和poll更高效，特别是在大量文件描述符时。
 */
DEFINE_SYSCALL (epoll_create, (int size))
{
    return vfs_epoll_create (size);
}
