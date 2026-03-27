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


/**
 * @brief 系统调用实现：创建管道。
 *
 * 该函数实现了一个系统调用，用于创建一个单向数据通道（管道）。
 * 管道可用于进程间通信，一个进程从管道的一端写入数据，另一个进程从另一端读取数据。
 *
 * @param[out] fildes 整数数组，用于返回两个文件描述符：
 *                   - fildes[0] 用于从管道读取数据
 *                   - fildes[1] 用于向管道写入数据
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功创建管道。
 * @retval -EFAULT fildes指向的地址无效。
 * @retval -EMFILE 进程已达到打开文件描述符的限制。
 * @retval -ENFILE 系统已达到打开文件的限制。
 * @retval -ENOMEM 内存不足。
 */
DEFINE_SYSCALL (pipe, (int __user *fildes))
{
    return SYSCALL_FUNC (pipe2) (fildes, 0);
}
