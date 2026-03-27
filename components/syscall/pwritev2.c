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
#include <fs/fs.h>
#include <stdlib.h>

/**
 * @brief 系统调用实现：将数据写入文件描述符。
 *
 * 该函数实现了一个系统调用，用于将用户空间的数据写入到指定的文件描述符中。
 *
 * @param fd 文件描述符，表示要写入的文件。
 * @param buf 指向用户空间缓冲的指针，包含要写入的数据。
 * @param count 要写入的数据字节数。
 * @return 成功时返回写入的字节数，失败时返回负值错误码。
 *
 * @note 如果 `count` 为 0，则函数直接返回 0，表示没有数据需要写入。
 * @note 如果内存分配失败，函数返回 `-ENOMEM`。
 * @note 如果从用户空间复制数据失败，函数返回相应的错误码。
 * @note 最终调用 `vfs_write` 函数执行实际的写入操作，并释放分配的内核缓冲区。
 */
DEFINE_SYSCALL (pwritev2, (unsigned long fd, const struct iovec __user *vec,
                           unsigned long vlen, unsigned long pos_l,
                           unsigned long pos_h,int flags))
{
    int  i;
    long total = 0;
    loff_t offset = ((uint64_t)pos_l & 0xffffffff) | ((uint64_t)pos_h << 32);
    size_t write_bytes;
    if ((vec == NULL) || (vlen <= 0))
        return -EINVAL;
    //todo other flag
    for (i = 0; i < vlen; i++)
    {
        if(RWF_APPEND & flags)
        {
            lseek(fd, 0, SEEK_END);
            total += SYSCALL_FUNC(write)(fd, vec->iov_base, vec->iov_len);
        }
        else
        {
            write_bytes = SYSCALL_FUNC (pwrite64) (fd, vec->iov_base, vec->iov_len, offset);
            total += write_bytes;
            offset += write_bytes;
        }
        
        vec++;
    }

    return total;
}
