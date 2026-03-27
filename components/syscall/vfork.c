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
#include <assert.h>
#include <signal.h>
#include <ttosBase.h>
#include <ttosProcess.h>

/**
 * @brief 系统调用实现：创建共享父进程内存空间的子进程。
 *
 * 该函数实现了一个系统调用，用于创建一个新的子进程，该子进程与父进程共享内存空间，
 * 直到子进程调用exec()或exit()。在此期间，父进程被挂起。
 *
 * @return 在父进程中返回子进程的PID，在子进程中返回0，失败时返回负值错误码。
 * @retval >0 在父进程中返回子进程的PID。
 * @retval 0 在子进程中返回。
 * @retval -EAGAIN 进程数量达到系统限制。
 * @retval -ENOMEM 内存不足。
 *
 * @note 1. vfork()创建的子进程与父进程共享内存空间。
 *       2. 父进程在子进程调用exec()或exit()之前被挂起。
 *       3. 子进程不应该返回或使用return语句，应该调用_exit()退出。
 *       4. 子进程不应该修改共享的数据，除非在exec()之前退出。
 */
DEFINE_SYSCALL (vfork, (void))
{
    return do_fork (CLONE_VM | CLONE_VFORK | SIGCHLD, 0, NULL, NULL, 0, NULL);
}