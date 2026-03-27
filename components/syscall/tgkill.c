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
#include <list.h>
#include <process_signal.h>
#include <ttosProcess.h>
#include <uaccess.h>

/**
 * @brief 系统调用实现：向线程组中的指定线程发送信号。
 *
 * 该函数实现了一个系统调用，用于向指定线程组中的特定线程发送信号。
 *
 * @param[in] tgid 目标线程组ID
 * @param[in] pid 目标线程ID
 * @param[in] sig 要发送的信号
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功发送信号。
 * @retval -EINVAL pid不是tgid指定的线程组中的线程。
 * @retval -ESRCH 指定的线程不存在。
 * @retval -EPERM 权限不足。
 *
 * @note 1. 只能向同一线程组中的线程发送信号。
 *       2. 需要适当的权限。
 *       3. 线程必须存在且可被发送信号。
 */
DEFINE_SYSCALL(tgkill, (pid_t tgid, pid_t pid, int sig))
{
    /* 判断pid是否是在tgid中 */
    pcb_t pcb = pcb_get_by_pid(tgid);
    pcb_t thread;
    int found = 0;
    int ret = 0;

    for_each_thread_in_tgroup(pcb, thread)
    {
        if (thread->taskControlId->tid == pid)
        {
            found = 1;
            break;
        }
    }

    if (found)
    {
        ret = kernel_signal_send(pid, TO_THREAD, sig, SI_USER, NULL);
    }
    else
    {
        /* pid不是tgid中的线程 */
        ret = -EINVAL;
    }

    return ret;
}
