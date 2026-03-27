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
#include <sys/uio.h>

/**
 * @brief 系统调用实现：从文件描述符读取数据到多个缓冲区。
 *
 * 该函数实现了一个系统调用，用于从文件描述符读取数据到多个不连续的缓冲区中。
 * 这种操作也称为分散读取（scatter read）。
 *
 * @param[in] fd 要读取的文件描述符。
 * @param[in] iov iovec结构数组，每个结构指定一个缓冲区：
 *               - iov_base: 缓冲区的起始地址
 *               - iov_len: 缓冲区的长度
 * @param[in] iovcnt iov数组中的元素个数。
 * @return 成功时返回读取的字节总数，失败时返回负值错误码。
 * @retval >0 实际读取的字节数。
 * @retval 0 到达文件末尾。
 * @retval -EBADF fd不是有效的文件描述符。
 * @retval -EFAULT iov指向无效内存。
 * @retval -EINVAL iovcnt无效或某个iov_len无效。
 * @retval -EISDIR fd引用一个目录。
 * @retval -EAGAIN 非阻塞I/O操作会阻塞。
 * @retval -EINTR 操作被信号中断。
 * @retval -EIO I/O错误。
 */
DEFINE_SYSCALL (readv, (unsigned long fd, const struct iovec __user *vec,
                        unsigned long vlen))
{
    int  i;
    long total = 0;
    for (i = 0; i < vlen; i++)
    {
        total += SYSCALL_FUNC (read) (fd, vec->iov_base, vec->iov_len);
        vec++;
    }
    return total;
}
