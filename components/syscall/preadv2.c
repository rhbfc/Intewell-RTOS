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
#include <malloc.h>
#include <fs/kpoll.h>
#include <ttos.h>

/**
 * @brief 系统调用实现：从文件读取数据。
 *
 * 该函数实现了一个系统调用，用于从文件读取数据。
 *
 * @param fd 文件描述符。
 * @param vec 指向用户空间的 I/O 向量结构体指针。
 * @param vlen I/O 向量的数量。
 * @param pos_l 读取位置的低 32 位。
 * @param pos_h 读取位置的高 32 位。
 * @param flags 读取标志。
 * @return 成功时返回读取的字节数，失败时返回负值错误码。
 *
 * @note 如果 `vlen` 为 0，则函数直接返回 0，表示没有数据需要读取。
 * @note 如果内存分配失败，函数返回 `-ENOMEM`。
 * @note 如果从用户空间复制数据失败，函数返回相应的错误码。
 * @note 最终调用 `vfs_preadv` 函数执行实际的读取操作，并释放分配的内核缓冲区。
 */
DEFINE_SYSCALL (preadv2, (unsigned long fd, const struct iovec __user *vec,
			    unsigned long vlen, unsigned long pos_l, unsigned long pos_h,
			    int flags))
{
    int  i;
    long total = 0;
    size_t read_bytes;
    loff_t offset = ((uint64_t)pos_l & 0xffffffff) | ((uint64_t)pos_h << 32);
    for (i = 0; i < vlen; i++)
    {
        //todo other flag
        if((flags & RWF_NOWAIT) == 0)
        {
            struct kpollfd pfd;
            pfd.pollfd.fd     = fd;
            pfd.pollfd.events = POLLIN;
            int ret;
            ret = kpoll (&pfd, 1, TTOS_WAIT_FOREVER);
            if (ret < 0)
            {
                return -EAGAIN;
            }
        }
        read_bytes = SYSCALL_FUNC (pread64) (fd, vec->iov_base, vec->iov_len, offset);
        total += read_bytes;
        offset += read_bytes;
        vec++;
    }
    return total;
}
