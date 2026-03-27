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
#include <signal_internal.h>
#include <ttos.h>
#include <uaccess.h>
#include <ttosProcess.h>
#include <errno.h>

/**
 * @brief 系统调用实现：获取待处理信号集。
 *
 * 该函数实现了一个系统调用，用于获取当前进程中被阻塞的待处理信号集。
 *
 * @param[out] set 用于存储待处理信号集的缓冲区
 * @param[in] sigsetsize 信号集大小
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功。
 * @retval -EINVAL 参数无效。
 * @retval -EFAULT 内存访问错误。
 * @retval -ENOSYS 不支持的操作。
 *
 * @note 1. 支持实时信号。
 *       2. 线程安全。
 *       3. 进程级操作。
 *       4. 不会阻塞。
 */
DEFINE_SYSCALL (rt_sigpending, (sigset_t __user * set, size_t sigsetsize))
{
    k_sigset_t pending;
    if (sizeof(k_sigset_t) < sigsetsize)
    {
        return -EINVAL;
    }

    kernel_signal_pending(ttosProcessSelf(), &pending);

    if (copy_to_user(set, &pending, sigsetsize))
    {
        return -EFAULT;
    }

    return 0;
}
