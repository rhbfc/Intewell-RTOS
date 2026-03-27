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
 * @brief 系统调用实现：接受一个新的网络连接。
 *
 * 该函数实现了一个系统调用，用于在一个监听套接字上接受新的连接。
 * 它从监听套接字的等待队列中提取第一个连接请求，创建一个新的已连接套接字，
 * 并返回指向该套接字的文件描述符。
 *
 * @param[in] sockfd 监听套接字的文件描述符。
 * @param[out] addr 指向sockaddr结构的指针，用于返回对端的地址信息。
 * @param[in,out] addrlen 指向socklen_t的指针，输入时表示addr缓冲区的大小，
 *                       输出时表示实际存储的地址长度。
 * @param[in] flags 控制套接字行为的标志：
 *              - SOCK_NONBLOCK: 将新套接字设置为非阻塞模式
 *              - SOCK_CLOEXEC: 在执行exec时关闭新套接字
 * @return 成功时返回新套接字的文件描述符，失败时返回负值错误码。
 * @retval >=0 新接受连接的套接字文件描述符。
 * @retval -EBADF sockfd不是有效的文件描述符。
 * @retval -ENOTSOCK sockfd不是套接字。
 * @retval -EOPNOTSUPP 套接字类型不支持accept操作。
 * @retval -EINVAL socket未处于监听状态。
 * @retval -EFAULT addr或addrlen指向无效内存。
 * @retval -EWOULDBLOCK 套接字为非阻塞且没有等待的连接。
 * @retval -EMFILE 进程已达到打开文件描述符的限制。
 * @retval -ENFILE 系统已达到打开文件的限制。
 * @retval -ENOBUFS 没有足够的空闲内存。
 * @retval -ENOMEM 内存不足。
 * @retval -EPROTO 协议错误。
 */
DEFINE_SYSCALL (accept4, (int fd, struct sockaddr __user *upeer_sockaddr,
                           int __user *upeer_addrlen, int flags))
{
    struct sockaddr *kpeer_sockaddr = NULL;
    socklen_t        kpeer_addrlen  = 0;
    int              ret;
    int              new_fd;

    if ((NULL == upeer_sockaddr && NULL != upeer_addrlen)
        || (NULL != upeer_sockaddr && NULL == upeer_addrlen))
    {
        return -EINVAL;
    }

    if (flags & ~(SOCK_NONBLOCK | SOCK_CLOEXEC))
    {
        return -EINVAL;
    }

    if (fd < 0)
    {
        return -EBADF;
    }

    if (upeer_addrlen)
    {
        ret = copy_from_user (&kpeer_addrlen, upeer_addrlen, sizeof (int));
        if (ret < 0)
        {
            return ret;
        }

        if (kpeer_addrlen <= 0)
        {
            return -EINVAL;
        }
        else
        {
            kpeer_sockaddr = (struct sockaddr *)malloc (kpeer_addrlen);
        }

        if (NULL == kpeer_sockaddr)
        {
            return -ENOMEM;
        }
    }

    new_fd = vfs_accept4 (fd, kpeer_sockaddr,
                          upeer_addrlen ? &kpeer_addrlen : NULL, flags);
    if (new_fd < 0)
    {
        ret = new_fd;
        goto out;
    }

    if (upeer_addrlen)
    {
        ret = copy_to_user (upeer_sockaddr, kpeer_sockaddr, kpeer_addrlen);
        if (ret < 0)
        {
            if (new_fd)
            {
                close (new_fd);
            }
            goto out;
        }

        ret = copy_to_user (upeer_addrlen, &kpeer_addrlen, sizeof (int));
        if (ret < 0)
        {
            if (new_fd)
            {
                close (new_fd);
            }
            goto out;
        }
    }

    ret = new_fd;

out:

    if (kpeer_sockaddr)
    {
        free (kpeer_sockaddr);
    }

    return ret;
}
