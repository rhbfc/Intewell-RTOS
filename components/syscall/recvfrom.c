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
 * @brief 系统调用实现：从套接字接收数据。
 *
 * 该函数实现了一个系统调用，用于从套接字接收数据并获取发送方地址。
 *
 * @param[in] sockfd 套接字文件描述符
 * @param[out] buf 接收数据的缓冲区
 * @param[in] len 缓冲区长度
 * @param[in] flags 接收标志：
 *                  - MSG_PEEK：查看数据
 *                  - MSG_WAITALL：等待所有数据
 *                  - MSG_DONTWAIT：非阻塞
 * @param[out] src_addr 发送方地址结构
 * @param[in,out] addrlen 地址结构长度
 * @return 成功时返回接收的字节数，失败时返回负值错误码。
 * @retval >0 成功接收的字节数。
 * @retval 0 连接已关闭。
 * @retval -EBADF 无效的文件描述符。
 * @retval -EINVAL 参数无效。
 *
 * @note 1. 可能阻塞。
 *       2. 支持多协议。
 *       3. 地址可选。
 *       4. 线程安全。
 */
DEFINE_SYSCALL (recvfrom,
                (int fd, void __user *ubuf, size_t size, unsigned flags,
                 struct sockaddr __user *addr, socklen_t __user *addr_len))
{
    ssize_t          recv_len;
    if (!user_access_check (ubuf, size, UACCESS_W))
    {
        return -EINVAL;
    }

    recv_len = vfs_recvfrom (fd, ubuf, size, flags, addr, addr_len);

    return recv_len;
}
