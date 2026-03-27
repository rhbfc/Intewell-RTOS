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
 * @brief 系统调用实现：获取套接字本地地址。
 *
 * 该函数实现了一个系统调用，用于获取套接字的本地地址。
 *
 * @param[in] sockfd 套接字文件描述符
 * @param[out] addr 存储本地地址的结构体
 * @param[in,out] addrlen 地址结构体的长度
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功获取本地地址。
 * @retval -EBADF sockfd不是有效的文件描述符。
 * @retval -EFAULT addr或addrlen指向无效内存。
 * @retval -ENOTSOCK sockfd不是套接字。
 * @retval -EINVAL addrlen值无效。
 *
 * @note 1. 适用于所有类型的套接字。
 *       2. 对于未绑定的套接字，返回通配地址。
 *       3. addr的实际类型取决于协议族。
 *       4. addrlen会被更新为实际写入的长度。
 */
DEFINE_SYSCALL (getsockname, (int sockfd, struct sockaddr __user *addr,
                              socklen_t __user *addrlen))
{

    struct sockaddr *kaddr;
    socklen_t       kaddrlen;
    int             ret;

    ret = copy_from_user(&kaddrlen, addrlen, sizeof(socklen_t));
    if (ret < 0)
    {
        return 0;
    }

    kaddr = malloc(kaddrlen);
    if(kaddr == NULL)
    {
        return -ENOMEM;
    }

    ret = vfs_getsockname (sockfd, kaddr, &kaddrlen);
    if (ret < 0)
    {
        goto out;
    }

    ret = copy_to_user (addr, kaddr, kaddrlen);
    if (ret < 0)
    {
        goto out;
    }

    ret = copy_to_user (addrlen, &kaddrlen, sizeof (socklen_t));

out: 
    free(kaddr);
    return ret;
}
