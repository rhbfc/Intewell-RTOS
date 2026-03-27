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
#include <process_signal.h>
#include <ttosProcess.h>
#include <uaccess.h>

/**
 * @brief 系统调用实现：等待信号。
 *
 * 该函数实现了一个系统调用，用于临时替换信号屏蔽字并挂起进程直到收到信号。
 *
 * @param[in] unewset 临时的信号屏蔽字
 * @param[in] sigsetsize 信号集大小
 * @return 总是返回-EINTR。
 * @retval -EINTR 被信号中断。
 * @retval -EINVAL 参数无效。
 * @retval -EFAULT 内存访问错误。
 *
 * @note 1. 支持实时信号。
 *       2. 原子操作。
 *       3. 会被阻塞。
 *       4. 自动恢复。
 */
DEFINE_SYSCALL (rt_sigsuspend, (sigset_t __user *unewset, size_t sigsetsize))
{
    k_sigset_t kset = {0};

    if (sigsetsize != sizeof (kset))
    {
        KLOG_EMERG ("rt_sigsuspend:sigsetsize != sizeof (kset) !!!");
        return -EINVAL;
    }

    if (!user_access_check (unewset, sigsetsize, UACCESS_R))
    {
        return -EINVAL;
    }

    if (copy_from_user (&kset, unewset, sigsetsize))
    {
        return -EFAULT;
    }

    kernel_signal_suspend(&kset);

    return -ERR_RESTART_IF_NO_HANDLER;
}
