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
#include <unistd.h>

/**
 * @brief 系统调用实现：获取进程组ID。
 *
 * 该函数实现了一个系统调用，用于获取指定进程的进程组ID。
 *
 * @param[in] pid 进程ID，如果为0则获取调用进程的进程组ID
 * @return 成功时返回进程组ID，失败时返回负值错误码。
 * @retval >0 成功，返回进程组ID。
 * @retval -ESRCH 指定的进程不存在。
 * @retval -EPERM 没有权限访问指定进程。
 *
 * @note 1. 进程组用于作业控制。
 *       2. 每个进程都属于一个进程组。
 *       3. 进程组ID通常等于组长进程的PID。
 *       4. 可用于向整个进程组发送信号。
 */
DEFINE_SYSCALL (getpgid, (pid_t pid))
{
    pcb_t pcb;
    if (pid == 0)
    {
        pcb = ttosProcessSelf ();
    }
    else
    {
        pcb = pcb_get_by_pid (pid);
    }

    return pcb ? pcb->pgid:-ESRCH;
}