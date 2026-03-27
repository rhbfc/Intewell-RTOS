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
#include <poll.h>
#include <signal.h>
#include <restart.h>

/**
 * @brief 系统调用实现：等待文件描述符事件（带信号掩码）。
 *
 * 该函数实现了一个系统调用，用于带信号掩码地监视多个文件描述符的事件。
 *
 * @param[in,out] fds 要监视的文件描述符数组：
 *                    - fd：文件描述符
 *                    - events：请求的事件
 *                    - revents：返回的事件
 * @param[in] nfds 数组中的元素个数
 * @param[in] tsp 超时时间结构：
 *                - tv_sec：秒数
 *                - tv_nsec：纳秒数
 * @param[in] sigmask 临时信号掩码
 * @return 成功时返回就绪的描述符数，失败时返回负值错误码。
 * @retval >0 有描述符就绪。
 * @retval 0 超时。
 * @retval -EFAULT 内存访问错误。
 * @retval -EINTR 被信号中断。
 *
 * @note 1. 支持纳秒级超时。
 *       2. 可临时修改信号掩码。
 *       3. 支持多种事件。
 *       4. 线程安全。
 */
DEFINE_SYSCALL (ppoll, (struct pollfd __user * ufds, unsigned int nfds,
                         struct timespec __user *tsp,
                         const sigset_t __user *sigmask, size_t sigsetsize))
{
    long ready;
    sigset_t origmask;
    int timeout = -1;

    if (sigsetsize != sizeof(k_sigset_t))
    {
        return -EINVAL;
    }

    /* 用户态传进来的 时间戳是两个long的形式 */
    if(tsp)
    {
        timeout = (((long __user *)(tsp))[0] * 1000 + ((long __user *)(tsp))[1] / 1000000);
    }

    if(sigmask)
    {
        SYSCALL_FUNC (rt_sigprocmask) (SIG_SETMASK, sigmask,
                                 &origmask, sigsetsize);
    }

    /* 如果这四个参数为0，则表示执行pause功能 */
    if (!ufds && !nfds && !tsp && !sigmask)
    {
        SYSCALL_FUNC (pause)();
        return -ERR_RESTART_IF_NO_HANDLER;
    }

    ready = SYSCALL_FUNC(poll)(ufds, nfds, timeout);

    if(sigmask)
    {
        SYSCALL_FUNC (rt_sigprocmask) (SIG_SETMASK, &origmask,
                                 NULL, sigsetsize);
    }

    return ready;
}
