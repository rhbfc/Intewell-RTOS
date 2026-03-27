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
#include <uaccess.h>
#include <signal.h>

/**
 * @brief 系统调用实现：暂停进程执行。
 *
 * 该函数实现了一个系统调用，用于使当前进程暂停执行直到收到信号。
 *
 * @return 总是返回-EINTR。
 * @retval -EINTR 被信号中断。
 *
 * @note 1. 进程会一直阻塞。
 *       2. 只能被信号唤醒。
 *       3. 常用于等待事件。
 *       4. 总是返回-EINTR。
 */
DEFINE_SYSCALL (pause, (void))
{
    k_sigset_t set = {0};
    pcb_t pcb = ttosProcessSelf();

    memcpy(&set, &pcb->blocked, sizeof(set));
    kernel_signal_suspend (&set);

    return -ERR_RESTART_IF_NO_HANDLER;
}