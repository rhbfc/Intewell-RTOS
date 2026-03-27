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
 * @brief 系统调用实现：获取套接字选项。
 *
 * 该函数实现了一个系统调用，用于获取套接字的各种选项值。
 *
 * @param[in] sockfd 套接字文件描述符。
 * @param[in] level 协议层次：
 *              - SOL_SOCKET: 套接字层
 *              - IPPROTO_IP: IP协议层
 *              - IPPROTO_TCP: TCP协议层
 *              - IPPROTO_UDP: UDP协议层
 * @param[in] optname 选项名：
 *              SOL_SOCKET层：
 *              - SO_ACCEPTCONN: 套接字是否处于监听状态
 *              - SO_BROADCAST: 是否允许发送广播
 *              - SO_ERROR: 获取并清除套接字错误
 *              - SO_KEEPALIVE: 是否开启保活机制
 *              - SO_LINGER: 关闭时是否等待数据发送完成
 *              - SO_RCVBUF: 接收缓冲区大小
 *              - SO_SNDBUF: 发送缓冲区大小
 *              - SO_RCVTIMEO: 接收超时
 *              - SO_SNDTIMEO: 发送超时
 *              - SO_REUSEADDR: 是否允许地址重用
 * @param[out] optval 用于存储选项值的缓冲区。
 * @param[in,out] optlen 缓冲区长度的指针，返回时更新为实际长度。
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 获取选项成功。
 * @retval -EBADF sockfd不是有效的文件描述符。
 * @retval -ENOTSOCK sockfd不是套接字。
 * @retval -ENOPROTOOPT 不支持的选项。
 * @retval -EFAULT optval或optlen指向无效内存。
 */
DEFINE_SYSCALL (getsockopt, (int sockfd, int level, int optname,
                             char __user *optval, int __user *optlen))
{
    int       ret = 0;
    char     *koptval;
    socklen_t koptlen;

    if (optlen == NULL || optval == NULL)
    {
        return -EINVAL;
    }

    ret = copy_from_user (&koptlen, optlen, sizeof (int));
    if (ret < 0)
    {
        return ret;
    }
    koptval = malloc (koptlen);
    if (koptval == NULL)
    {
        return -ENOMEM;
    }
    ret = copy_from_user (koptval, optval, koptlen);
    if (ret < 0)
    {
        goto out;
    }

    ret = vfs_getsockopt (sockfd, level, optname, koptval, &koptlen);

    if (ret < 0)
    {
        goto out;
    }

    ret = copy_to_user (optlen, &koptlen, sizeof (int));
    if (ret < 0)
    {
        goto out;
    }

    ret = copy_to_user (optval, koptval, koptlen);
    if (ret < 0)
    {
        goto out;
    }

out:
    free (koptval);
    return ret;
}
