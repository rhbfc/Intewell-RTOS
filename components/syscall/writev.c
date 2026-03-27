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
 * @brief 系统调用实现：向文件写入多个缓冲区。
 *
 * 该函数实现了一个系统调用，用于将多个缓冲区的数据写入文件描述符。
 * 支持原子性写入多个不连续的内存区域。
 *
 * @param[in] fd 文件描述符
 * @param[in] iov iovec结构体数组，每个元素包含：
 *               - iov_base: 缓冲区地址
 *               - iov_len: 缓冲区长度
 * @param[in] iovcnt iovec数组的元素个数
 * @return 成功时返回写入的总字节数，失败时返回负值错误码。
 * @retval >0 实际写入的字节数。
 * @retval -EBADF fd不是有效的文件描述符。
 * @retval -EFAULT iov指向无效内存。
 * @retval -EINVAL 无效参数。
 * @retval -EINTR 写入操作被信号中断。
 * @retval -EIO I/O错误。
 * @retval -ENOSPC 设备空间不足。
 * @retval -EPIPE 写入到已关闭的管道。
 *
 * @note 1. 写入操作是原子的。
 *       2. 所有缓冲区都写入成功才返回。
 *       3. 写入顺序与iovec数组顺序相同。
 *       4. 总写入字节数可能小于请求的字节数。
 */
DEFINE_SYSCALL (writev, (unsigned long fd, const struct iovec __user *vec,
                         unsigned long vlen))
{
    int  i;
    long total = 0;

    if ((vec == NULL) || (vlen <= 0))
        return -EINVAL;

    for (i = 0; i < vlen; i++)
    {
        total += SYSCALL_FUNC (write) (fd, vec->iov_base, vec->iov_len);
        vec++;
    }

    return total;
}