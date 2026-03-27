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
 * @brief 系统调用实现：发送数据到指定地址。
 *
 * 该函数实现了一个系统调用，用于通过套接字发送数据到指定的目标地址。
 *
 * @param[in] fd 套接字文件描述符
 * @param[in] buf 要发送的数据缓冲区
 * @param[in] len 要发送的数据长度
 * @param[in] flags 发送标志：
 *                  - MSG_OOB：发送带外数据
 *                  - MSG_DONTROUTE：不路由
 *                  - MSG_DONTWAIT：非阻塞
 *                  - MSG_NOSIGNAL：不产生SIGPIPE信号
 * @param[in] addr 目标地址结构体
 * @param[in] addr_len 地址结构体长度
 * @return 成功时返回发送的字节数，失败时返回负值错误码。
 * @retval >0 成功发送的字节数。
 * @retval -EBADF 无效的文件描述符。
 * @retval -EINVAL 参数无效。
 * @retval -EFAULT 内存访问错误。
 * @retval -EAGAIN 资源暂时不可用。
 *
 * @note 1. 支持多种协议。
 *       2. 支持多种地址。
 *       3. 支持多种标志。
 *       4. 可能阻塞。
 */
DEFINE_SYSCALL (sendto, (int fd, void __user *buff, size_t len, unsigned flags,
                         struct sockaddr __user *addr, socklen_t addr_len))

{
    struct sockaddr *kaddr = NULL;
    void            *kbuff = NULL;
    int                ret;
    ssize_t       send_len;

    if (addr_len < 0)
    {
        return -EINVAL;
    }
    else if (addr_len != 0)
    {
        kaddr = (struct sockaddr *)malloc (addr_len);
    }

    if (addr_len != 0)
    {
        if (kaddr == NULL)
        {
            return -ENOMEM;
        }

        if (addr != NULL)
        {
            ret = copy_from_user (kaddr, addr, addr_len);
            if (ret < 0)
            {
                send_len = (ssize_t)ret;
                goto out;
            }
        }
    }
    else
    {
        if (addr != NULL)
        {
            send_len = -EISCONN;
            goto out;
        }
    }

    kbuff = malloc (len);
    if (kbuff == NULL)
    {
        send_len = -ENOMEM;
        goto out;
    }

    ret = copy_from_user (kbuff, buff, len);
    if (ret < 0)
    {
        send_len = (ssize_t)ret;
        goto out;
    }

    send_len = vfs_sendto (fd, kbuff, len, flags, kaddr, addr_len);

out:

    if (kaddr)
    {
        free (kaddr);
    }
    if (kbuff)
    {
        free (kbuff);
    }

    return send_len;
}
