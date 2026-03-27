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
#include <sys/socket.h>
#include <net/net.h>

/**
 * @brief 系统调用实现：监听连接请求。
 *
 * 该函数实现了一个系统调用，用于将一个套接字标记为被动套接字，
 * 也就是说，该套接字将被用来接受来自其他套接字的连接请求。
 *
 * @param[in] sockfd 套接字文件描述符。
 * @param[in] backlog 等待连接队列的最大长度。
 *                   实际队列长度可能会被系统限制在一个较小的值。
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 监听成功。
 * @retval -EBADF sockfd不是有效的文件描述符。
 * @retval -ENOTSOCK sockfd不是套接字。
 * @retval -EOPNOTSUPP 套接字不支持监听操作。
 * @retval -EADDRINUSE 另一个套接字已经在监听相同的端口。
 * @retval -EINVAL 套接字未绑定到本地地址或backlog参数无效。
 * @retval -EACCES 权限不足。
 * @retval -EINVAL sockfd已经在监听状态。
 */
DEFINE_SYSCALL (listen, (int sockfd, int backlog))
{
    return vfs_listen (sockfd, backlog);
}