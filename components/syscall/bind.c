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
 * @brief 系统调用实现：将套接字绑定到地址。
 *
 * 该函数实现了一个系统调用，用于将一个套接字绑定到一个特定的地址。
 * 对于网络套接字，这通常意味着将套接字与一个本地端口号关联。
 *
 * @param[in] sockfd 套接字文件描述符。
 * @param[in] addr 要绑定的地址结构指针。
 * @param[in] addrlen 地址结构的长度。
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 绑定成功。
 * @retval -EBADF sockfd不是有效的文件描述符。
 * @retval -ENOTSOCK sockfd不是套接字。
 * @retval -EINVAL 地址长度错误或套接字已绑定。
 * @retval -EACCES 地址受保护。
 * @retval -EADDRINUSE 地址已被使用。
 * @retval -EFAULT addr指向无效内存。
 * @retval -EADDRNOTAVAIL 接口不存在或请求的地址不合法。
 * @retval -EAFNOSUPPORT 地址族不被支持。
 * @retval -ELOOP 解析地址时遇到太多符号链接。
 * @retval -ENAMETOOLONG 地址太长。
 * @retval -ENOENT 文件不存在。
 * @retval -ENOMEM 内存不足。
 * @retval -ENOTDIR 路径组件不是目录。
 */
DEFINE_SYSCALL (bind, (int fd, struct sockaddr __user *umyaddr, int addrlen))
{
    struct sockaddr *kmyaddr;
    int              ret = 0;

    if (fd < 0)
    {
        return -EBADF;
    }

    if (addrlen <= 0 || NULL == umyaddr)
    {
        return -EINVAL;
    }

    kmyaddr = (struct sockaddr *)malloc (addrlen);

    if (kmyaddr == NULL)
    {
        return -ENOMEM;
    }

    ret = copy_from_user (kmyaddr, umyaddr, addrlen);
    if (ret < 0)
    {
        goto out;
    }

    ret = vfs_bind (fd, kmyaddr, addrlen);

out:

    free (kmyaddr);

    return ret;
}
