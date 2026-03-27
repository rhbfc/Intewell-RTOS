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
#include <uaccess.h>
#include <ttosProcess.h>

/**
 * @brief 系统调用实现：获取文件系统统计信息
 *
 * 该函数实现了一个系统调用，用于获取文件系统统计信息
 *
 * @param[in] fd 文件描述符。
 * @param[in] sz 结构体statfs64大小。
 * @param[out] statbuf 指向用户空间的文件系统状态结构体指针。
 * @return 成功时返回 0，失败时返回负值错误码。
 * @retval -EBADF: 文件夹描述符无效。 
 * @retval -EFAULT: 缓冲区指向无效地址
 * @retval -EINTR: 调用被信号中断
 * @retval -EIO: 发生了I/O错误
 * @retval -ENOMEM: 内存不足。
 * @retval -ENOSYS: 文件系统不支持此调用。
 * @retval -EOVERFLOW: 一些值太大，无法在返回的结构中表示
 */
DEFINE_SYSCALL (fstatfs64, (int fd, size_t sz, struct statfs64 __user *buf))
{
    if(sz < sizeof(struct statfs))
    {
        return -EINVAL;
    }

    return SYSCALL_FUNC(fstatfs) (fd, buf);
}