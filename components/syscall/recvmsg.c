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
#include <uaccess.h>
/**
 * @brief 系统调用实现：从套接字接收消息。
 *
 * 该函数实现了一个系统调用，用于从套接字接收消息及其控制信息。
 *
 * @param[in] sockfd 套接字文件描述符
 * @param[in,out] msg msghdr结构，包含：
 *                    - msg_name：发送方地址
 *                    - msg_iov：数据缓冲区数组
 *                    - msg_control：控制信息
 * @param[in] flags 接收标志：
 *                  - MSG_PEEK：查看数据
 *                  - MSG_WAITALL：等待所有数据
 *                  - MSG_DONTWAIT：非阻塞
 * @return 成功时返回接收的字节数，失败时返回负值错误码。
 * @retval >0 成功接收的字节数。
 * @retval 0 连接已关闭。
 * @retval -EBADF 无效的文件描述符。
 * @retval -EINVAL 参数无效。
 *
 * @note 1. 支持分散接收。
 *       2. 可获取控制信息。
 *       3. 可能阻塞。
 *       4. 线程安全。
 */
DEFINE_SYSCALL(recvmsg, (int fd, struct msghdr __user *msg, unsigned flags))
{
    if (NULL == msg)
    {
        return -EINVAL;
    }

    return vfs_recvmsg(fd, msg, flags);
}
