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
 * @brief 系统调用实现：设置套接字选项。
 *
 * 该函数实现了一个系统调用，用于设置套接字的各种选项值。
 *
 * @param[in] fd 套接字文件描述符。
 * @param[in] level 协议层次：
 *              - SOL_SOCKET: 套接字层
 *              - IPPROTO_IP: IP协议层
 *              - IPPROTO_TCP: TCP协议层
 *              - IPPROTO_UDP: UDP协议层
 * @param[in] optname 选项名：
 *              SOL_SOCKET层：
 *              - SO_BROADCAST: 允许发送广播
 *              - SO_DEBUG: 打开调试跟踪
 *              - SO_DONTROUTE: 不查找路由
 *              - SO_KEEPALIVE: 保持连接
 *              - SO_LINGER: 延迟关闭
 *              - SO_OOBINLINE: 带外数据放入正常数据流
 *              - SO_RCVBUF: 接收缓冲区大小
 *              - SO_SNDBUF: 发送缓冲区大小
 *              - SO_RCVLOWAT: 接收缓冲区下限
 *              - SO_SNDLOWAT: 发送缓冲区下限
 *              - SO_RCVTIMEO: 接收超时
 *              - SO_SNDTIMEO: 发送超时
 *              - SO_REUSEADDR: 允许地址重用
 * @param[in] optval 指向包含新选项值的缓冲区。
 * @param[in] optlen 选项值的长度。
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 设置选项成功。
 * @retval -EBADF fd不是有效的文件描述符。
 * @retval -ENOTSOCK fd不是套接字。
 * @retval -ENOPROTOOPT 不支持的选项。
 * @retval -EFAULT optval指向无效内存。
 * @retval -EINVAL 选项值无效。
 */
DEFINE_SYSCALL (setsockopt, (int fd, int level, int optname,
                             char __user *optval, int optlen))
{
    int   ret = 0;
    char *koptval;

    if (optval == NULL)
    {
        return -EINVAL;
    }

    koptval = malloc (optlen);
    if (koptval == NULL)
    {
        return -ENOMEM;
    }
    ret = copy_from_user (koptval, optval, optlen);
    if (ret < 0)
    {
        goto out;
    }

    ret = vfs_setsockopt (fd, level, optname, koptval, optlen);

    if (ret < 0)
    {
        goto out;
    }

    ret = copy_to_user (optval, koptval, optlen);
    if (ret < 0)
    {
        goto out;
    }

out:
    free (koptval);
    return ret;
}
