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
#include <ttosProcess.h>
#include <errno.h>

/**
 * @brief 系统调用实现：创建子进程。
 *
 * 该函数实现了一个系统调用，用于创建调用进程的一个副本（子进程）。
 * 子进程与父进程几乎完全相同，但有以下区别：
 * - 子进程有自己唯一的进程ID
 * - 子进程的父进程ID是创建它的进程的ID
 * - 子进程不继承父进程的内存锁
 * - 子进程的资源使用量和CPU时间统计被设置为0
 * - 子进程不继承父进程的异步I/O操作和未决信号
 *
 * @return 对于父进程返回子进程ID，对于子进程返回0，失败时返回负值错误码。
 * @retval >0 在父进程中返回子进程的PID。
 * @retval 0 在子进程中返回。
 * @retval -EAGAIN 达到进程数或内存限制。
 * @retval -ENOMEM 内存不足。
 * @retval -ENOSYS 不支持fork操作。
 */
DEFINE_SYSCALL (fork, (void))
{
    return do_fork (SIGCHLD, 0, NULL, NULL, 0, NULL);
}