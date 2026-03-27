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
#include <stdlib.h>
#include <uaccess.h>

/**
 * @brief 系统调用实现：写入文件。
 *
 * 该函数实现了一个系统调用，用于向文件描述符写入数据。
 * 支持常规文件、设备文件、管道和套接字等。
 *
 * @param[in] fd 文件描述符
 * @param[in] buf 要写入的数据缓冲区
 * @param[in] count 要写入的字节数
 * @return 成功时返回实际写入的字节数，失败时返回负值错误码。
 * @retval >0 实际写入的字节数。
 * @retval -EBADF fd不是有效的文件描述符。
 * @retval -EFAULT buf指向无效内存。
 * @retval -EINTR 写入操作被信号中断。
 * @retval -EINVAL 无效参数。
 * @retval -EIO I/O错误。
 * @retval -ENOSPC 设备空间不足。
 * @retval -EPIPE 写入到已关闭的管道。
 * @retval -EAGAIN 非阻塞操作无法立即完成。
 *
 * @note 1. 写入操作从文件的当前位置开始。
 *       2. 对于常规文件，如果文件偏移量超过文件大小，文件会被扩展。
 *       3. 对于管道和套接字，写入可能会被阻塞。
 *       4. 实际写入的字节数可能小于请求的字节数。
 */
DEFINE_SYSCALL (write, (int fd, const char __user *buf, size_t count))
{
    /** 定义返回值变量 */
    ssize_t ret;
    /** 如果 count 为 0 */
    if (count == 0)
    {
        /** 返回 0 */
        return 0;
    }
    /** 分配内核缓冲区 */
    char *kbuf = memalign (64, count);
    /** 如果分配失败 */
    if (kbuf == NULL)
    {
        /** 返回 -ENOMEM 错误码 */
        return -ENOMEM;
    }
    /** 从用户空间复制数据到内核缓冲区 */
    ret = copy_from_user (kbuf, buf, count);
    {
        /** 如果复制失败 */
        if (ret < 0)
        {
            /** 返回错误码 */
            return ret;
        }
    }

    /** 调用 vfs_write 执行写入操作 */
    ret = vfs_write (fd, kbuf, count);

    /** 释放内核缓冲区 */
    free (kbuf);

    /** 返回写入的字节数或错误码 */
    return ret;
}