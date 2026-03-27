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
 * @brief 系统调用实现：建立到指定地址的连接。
 *
 * 该函数实现了一个系统调用，用于将一个套接字连接到指定的地址。
 * 对于面向连接的套接字（如SOCK_STREAM），这会建立一个实际的连接；
 * 对于无连接的套接字（如SOCK_DGRAM），这会设置默认的目标地址。
 *
 * @param[in] sockfd 套接字文件描述符。
 * @param[in] addr 目标地址的结构指针。
 * @param[in] addrlen 地址结构的长度。
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 连接成功。
 * @retval -EBADF sockfd不是有效的文件描述符。
 * @retval -ENOTSOCK sockfd不是套接字。
 * @retval -EAFNOSUPPORT 地址族不被支持。
 * @retval -EINVAL 地址长度无效或套接字状态无效。
 * @retval -EISCONN 套接字已连接。
 * @retval -ECONNREFUSED 没有监听的服务器。
 * @retval -ENETUNREACH 网络不可达。
 * @retval -ETIMEDOUT 连接超时。
 * @retval -EHOSTUNREACH 主机不可达。
 * @retval -EALREADY 套接字非阻塞且连接正在进行。
 * @retval -EINPROGRESS 套接字非阻塞且连接不能立即建立。
 * @retval -EAGAIN 没有更多的本地端口或内存不足。
 * @retval -EADDRINUSE 本地地址已在使用。
 * @retval -EACCES 用户没有权限。
 * @retval -EFAULT addr指向无效内存。
 */
DEFINE_SYSCALL (connect,
                 (int sockfd, struct sockaddr __user *addr, int addrlen))
{
    struct sockaddr *kmyaddr;
    int              ret = 0;

    if (addrlen < 0)
    {
        return -EINVAL;
    }
    if (addrlen == 0)
    {
        kmyaddr = malloc (sizeof(struct sockaddr));
    }
    else
    {
        kmyaddr = malloc (addrlen);
    }
    

    if (kmyaddr == NULL)
    {
        return -ENOMEM;
    }

    ret = copy_from_user (kmyaddr, addr, addrlen);
    if (ret < 0)
    {
        return ret;
    }

    ret = vfs_connect (sockfd, kmyaddr, addrlen);

    free (kmyaddr);

    return ret;
}
