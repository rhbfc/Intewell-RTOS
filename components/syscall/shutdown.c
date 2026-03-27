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
 * @brief 系统调用实现：关闭套接字的连接。
 *
 * 该函数实现了一个系统调用，用于关闭套接字的全部或部分连接。
 * 不同于close()，shutdown()可以选择性地关闭连接的读取和/或写入部分，
 * 而且shutdown()的效果是永久性的。
 *
 * @param[in] fd 套接字文件描述符。
 * @param[in] how 关闭方式：
 *              - SHUT_RD(0): 关闭读取部分
 *              - SHUT_WR(1): 关闭写入部分
 *              - SHUT_RDWR(2): 同时关闭读取和写入部分
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 关闭成功。
 * @retval -EBADF fd不是有效的文件描述符。
 * @retval -ENOTSOCK fd不是套接字。
 * @retval -ENOTCONN 套接字未连接。
 * @retval -EINVAL how参数无效。
 */
DEFINE_SYSCALL (shutdown, (int fd, int how))
{
    return vfs_shutdown (fd, how);
}