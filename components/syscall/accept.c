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

/**
 * @brief 系统调用实现：接受一个新的连接。
 *
 * 该函数实现了一个系统调用，用于接受一个新的连接。
 *
 * @param[in] fd 套接字文件描述符。
 * @param[in] upeer_sockaddr 指向用户空间的套接字地址结构体指针。
 * @param[in] upeer_addrlen 指向用户空间的套接字地址长度变量指针。
 * @return 成功时返回新的套接字文件描述符，失败时返回负值错误码。
 * @retval: -EAGAIN 套接字被标记为非阻塞，且没有连接可以被接受。
 * @retval: -EBADF  fd 不是一个打开的文件描述符。
 * @retval: -ECONNABORTED 连接被中断。
 * @retval: -EFAULT  addr 参数不在用户地址空间的可写部分。
 * @retval: -EINTR  被信号中断。
 * @retval: -EINVAL 参数错误。
 * @retval: -EMFILE 进程打开的文件描述符数量已达上限。
 * @retval: -ENFILE 系统打开的文件描述符数量已达上限。
 * @retval: -ENOBUFS 内存不足。
 * @retval: -ENOMEM 内存不足。
 * @retval: -ENOTSOCK fd 不是一个套接字。
 * @retval: -EOPNOTSUPP 套接字不支持 SOCK_STREAM 类型。
 * @retval: -EPROTO 协议错误。
 * @retval: -EWOULDBLOCK 套接字被标记为非阻塞，且没有连接可以被接受。
 *
 * @note 该函数实际上调用了 `accept4` 系统调用，flags 参数为 0。
 */
DEFINE_SYSCALL (accept, (int fd, struct sockaddr __user *upeer_sockaddr,
                         int __user *upeer_addrlen))
{
    /** 调用 accept4 系统调用 */
    return SYSCALL_FUNC (accept4) (fd, upeer_sockaddr, upeer_addrlen, 0);
}