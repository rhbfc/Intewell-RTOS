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
#include <errno.h>
#include <net/net.h>
#include <string.h>
#include <uaccess.h>

/**
 * @brief 系统调用实现：发送消息。
 *
 * 该函数实现了一个系统调用，用于通过套接字发送消息。
 *
 * @param[in] fd 套接字文件描述符
 * @param[in] msg 消息结构体
 * @param[in] flags 发送标志：
 *                  - MSG_OOB：发送带外数据
 *                  - MSG_DONTROUTE：不路由
 *                  - MSG_DONTWAIT：非阻塞
 *                  - MSG_NOSIGNAL：不产生SIGPIPE信号
 * @return 成功时返回发送的字节数，失败时返回负值错误码。
 * @retval >0 成功发送的字节数。
 * @retval -EBADF 无效的文件描述符。
 * @retval -EINVAL 参数无效。
 * @retval -EFAULT 内存访问错误。
 * @retval -EAGAIN 资源暂时不可用。
 *
 * @note 1. 支持多缓冲区。
 *       2. 支持控制信息。
 *       3. 支持多种标志。
 *       4. 可能阻塞。
 */
DEFINE_SYSCALL(sendmsg, (int fd, struct msghdr __user *msg, unsigned flags))
{
    struct msghdr *kmsg;
    int ret;
    ssize_t send_len;

    if (NULL == msg)
    {
        return -EINVAL;
    }

    kmsg = malloc(sizeof(*kmsg));
    if (NULL == kmsg)
    {
        return -ENOMEM;
    }

    ret = copy_from_user(kmsg, msg, sizeof(struct msghdr));
    if (ret < 0)
    {
        send_len = (ssize_t)ret;
        goto errout;
    }

    /* 上面copy时会copy出用户态地址，应首先置空，避免出错提前退出时释放错误 */
    kmsg->msg_name = NULL;
    kmsg->msg_iov = NULL;
    kmsg->msg_control = NULL;

    if (msg->msg_name)
    {
        kmsg->msg_name = malloc(msg->msg_namelen);
        if (kmsg->msg_name == NULL)
        {
            send_len = -ENOMEM;
            goto errout;
        }

        ret = copy_from_user(kmsg->msg_name, msg->msg_name, msg->msg_namelen);
        if (ret < 0)
        {
            send_len = (ssize_t)ret;
            goto errout;
        }
    }

    if (user_access_check(msg->msg_iov, msg->msg_iovlen * sizeof(struct iovec), UACCESS_R))
    {
        kmsg->msg_iov = calloc(kmsg->msg_iovlen, sizeof(struct iovec));
        if (kmsg->msg_iov == NULL)
        {
            send_len = -ENOMEM;
            goto errout;
        }
        for (int i = 0; i < kmsg->msg_iovlen; i++)
        {
            kmsg->msg_iov[i].iov_len = msg->msg_iov[i].iov_len;
            kmsg->msg_iov[i].iov_base = malloc(msg->msg_iov[i].iov_len);
            if (kmsg->msg_iov[i].iov_base == NULL)
            {
                send_len = -ENOMEM;
                goto errout;
            }
            ret = copy_from_user(kmsg->msg_iov[i].iov_base, msg->msg_iov[i].iov_base,
                                 msg->msg_iov[i].iov_len);
            if (ret < 0)
            {
                send_len = (ssize_t)ret;
                goto errout;
            }
        }
    }
    else
    {
        send_len = -EFAULT;
        goto errout;
    }

    if (msg->msg_control)
    {
        kmsg->msg_control = malloc(msg->msg_controllen);
        if (kmsg->msg_control == NULL)
        {
            send_len = -ENOMEM;
            goto errout;
        }
        ret = copy_from_user(kmsg->msg_control, msg->msg_control, msg->msg_controllen);
        if (ret < 0)
        {
            send_len = (ssize_t)ret;
            goto errout;
        }
    }

    send_len = vfs_sendmsg(fd, kmsg, flags);

errout:

    if (kmsg->msg_name)
    {
        free(kmsg->msg_name);
    }

    if (kmsg->msg_iov)
    {
        for (int i = 0; i < kmsg->msg_iovlen; i++)
        {
            if (kmsg->msg_iov[i].iov_base)
            {
                free(kmsg->msg_iov[i].iov_base);
            }
        }
        free(kmsg->msg_iov);
    }

    if (kmsg->msg_control)
    {
        free(kmsg->msg_control);
    }

    if (kmsg)
    {
        free(kmsg);
    }

    return send_len;
}
