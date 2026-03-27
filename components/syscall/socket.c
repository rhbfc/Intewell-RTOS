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
#include <net/net.h>

/**
 * @brief 系统调用实现：创建一个通信端点（套接字）。
 *
 * 该函数实现了一个系统调用，用于创建一个指定域、类型和协议的网络套接字。
 *
 * @param[in] family 协议族/地址族：
 *              - AF_UNIX, AF_LOCAL: 本地通信
 *              - AF_INET: IPv4网络协议
 *              - AF_INET6: IPv6网络协议
 *              - AF_IPX: IPX - Novell协议
 *              - AF_NETLINK: 用户态与内核通信
 *              - AF_PACKET: 底层包接口
 * @param[in] type 套接字类型：
 *              - SOCK_STREAM: 可靠的双向字节流
 *              - SOCK_DGRAM: 不可靠的数据报
 *              - SOCK_RAW: 原始套接字
 *              - SOCK_SEQPACKET: 可靠的数据报
 *              - SOCK_PACKET: 废弃的包接口
 * @param[in] protocol 具体协议：
 *              - 0: 使用默认协议
 *              - IPPROTO_TCP: TCP协议
 *              - IPPROTO_UDP: UDP协议
 *              - IPPROTO_SCTP: SCTP协议
 *              - IPPROTO_ICMP: ICMP协议
 * @return 成功时返回新套接字的文件描述符，失败时返回负值错误码。
 * @retval >=0 新创建的套接字文件描述符。
 * @retval -EAFNOSUPPORT 不支持请求的地址族。
 * @retval -EINVAL 无效的参数。
 * @retval -EMFILE 进程已达到打开文件描述符的限制。
 * @retval -ENFILE 系统已达到打开文件的限制。
 * @retval -ENOBUFS 内存不足，无法分配内核结构。
 * @retval -ENOMEM 内存不足。
 * @retval -EPROTONOSUPPORT 协议类型或指定的协议不被支持。
 */
DEFINE_SYSCALL (socket, (int family, int type, int protocol))
{
    return vfs_socket (family, type, protocol);
}
