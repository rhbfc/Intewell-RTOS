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
#include <fs/kpoll.h>
#include <stdlib.h>
#include <ttos.h>
#include <uaccess.h>

/**
 * @brief 系统调用实现：在文件描述符之间传输数据（64位）。
 *
 * 该函数实现了一个系统调用，用于在两个文件描述符之间直接传输数据，支持64位文件偏移。
 *
 * @param[in] out_fd 目标文件描述符
 * @param[in] in_fd 源文件描述符
 * @param[in,out] offset 源文件的64位偏移量，如果为NULL则使用当前偏移量
 * @param[in] count 要传输的字节数
 * @return 成功时返回传输的字节数，失败时返回负值错误码。
 * @retval >0 成功传输的字节数。
 * @retval -EBADF 无效的文件描述符。
 * @retval -EINVAL 参数无效。
 * @retval -EFAULT 内存访问错误。
 *
 * @note 1. 零拷贝传输。
 *       2. 支持大文件。
 *       3. 原子操作。
 *       4. 高效传输。
 */
DEFINE_SYSCALL (sendfile64,
                (int out_fd, int in_fd, loff_t __user *offset, size_t count))
{
   return SYSCALL_FUNC(splice)(in_fd, offset, out_fd, NULL, count, SPLICE_F_NONBLOCK);
}