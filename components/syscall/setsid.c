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
#include <sys/ioctl.h>
#include <ttosProcess.h>

/**
 * @brief 系统调用实现：创建新会话。
 *
 * 该函数实现了一个系统调用，用于创建一个新的会话，调用进程成为新会话的首进程。
 *
 * @return 成功时返回新会话的ID，失败时返回负值错误码。
 * @retval >0 新会话的ID。
 * @retval -EPERM 进程已经是进程组组长。
 *
 * @note 1. 创建新会话。
 *       2. 成为会话首进程。
 *       3. 脱离控制终端。
 *       4. 进程级操作。
 */
int setpgid(pid_t pid, pid_t pgid);

DEFINE_SYSCALL(setsid, (void))
{
    int ret;
    
    pcb_t pcb = ttosProcessSelf();
    pid_t pid = get_process_pid(pcb);
    pid_t pgid = pcb->pgid;
    if (pid == pgid)
    {
        /* 当前进程已经是进程组组长不得调用 setsid */
        return -EPERM;
    }

    /* 清除控制终端 */
    tty_clear_ctty(pcb);

    /* 创建新的进程组，并成为组长 */
    ret = setpgid(pid, 0);

    /* 设置会话id为当前进程的进程组号，也会与当前进程的pid相等 */
    pcb->sid = pcb->pgid;

    return ret == 0 ? pid : ret;
}