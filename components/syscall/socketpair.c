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
#include <uaccess.h>

/**
 * @brief 系统调用实现：创建一对相互连接的套接字。
 *
 * 该函数实现了一个系统调用，用于创建一对已连接的套接字。
 * 这对套接字可用于在同一台机器上的进程间进行双向通信。
 *
 * @param[in] family 协议族/地址族：
 *              - AF_UNIX, AF_LOCAL: 本地通信（唯一支持的域）
 * @param[in] type 套接字类型：
 *              - SOCK_STREAM: 可靠的双向字节流
 *              - SOCK_DGRAM: 不可靠的数据报
 * @param[in] protocol 具体协议（通常为0）。
 * @param[out] usockvec 整数数组，用于返回两个文件描述符：
 *               - usockvec[0] 和 usockvec[1] 是一对相互连接的套接字
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 创建成功。
 * @retval -EAFNOSUPPORT 不支持请求的地址族。
 * @retval -EFAULT usockvec指向无效内存。
 * @retval -EMFILE 进程已达到打开文件描述符的限制。
 * @retval -ENFILE 系统已达到打开文件的限制。
 * @retval -ENOBUFS 内存不足。
 * @retval -EOPNOTSUPP 不支持请求的套接字类型。
 */
DEFINE_SYSCALL (socketpair,
                (int family, int type, int protocol, int __user *usockvec))
{
    int sv[2];
    int ret = vfs_socketpair (family, type, protocol, sv);
    if (ret == 0)
    {
        ret = copy_to_user (usockvec, sv, sizeof (sv));
    }
    return ret;
}
