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
#include <sys/epoll.h>
#include <signal.h>

/**
 * @brief 系统调用实现：等待epoll事件（带信号屏蔽）。
 *
 * 该函数实现了一个系统调用，用于等待epoll实例上的事件发生。
 * 相比epoll_wait，支持在等待期间屏蔽指定信号。
 *
 * @param[in] epfd epoll实例的文件描述符
 * @param[out] events 返回的事件数组：
 *                  - events: 发生的事件掩码
 *                    - EPOLLIN: 可读
 *                    - EPOLLOUT: 可写
 *                    - EPOLLRDHUP: TCP连接对端关闭
 *                    - EPOLLPRI: 紧急数据可读
 *                    - EPOLLERR: 错误
 *                    - EPOLLHUP: 挂起
 *                  - data: 用户数据
 * @param[in] maxevents 最大事件数量
 * @param[in] timeout 超时时间（毫秒）：
 *                  - -1: 永久等待
 *                  - 0: 立即返回
 *                  - >0: 等待指定毫秒数
 * @param[in] sigmask 等待期间要屏蔽的信号集
 * @return 成功时返回就绪的文件描述符数量，失败时返回负值错误码。
 * @retval >0 有事件发生的文件描述符数量。
 * @retval 0 超时。
 * @retval -EBADF epfd不是有效的文件描述符。
 * @retval -EFAULT events或sigmask指向无效内存。
 * @retval -EINVAL 无效参数。
 * @retval -EINTR 等待被未屏蔽的信号中断。
 *
 * @note 1. 返回时events数组只包含有事件发生的描述符。
 *       2. 边缘触发模式下需要一次性读取所有数据。
 *       3. 返回的事件可能包含EPOLLERR或EPOLLHUP。
 *       4. 信号屏蔽只在等待期间有效。
 */
DEFINE_SYSCALL (epoll_pwait,
                 (int epfd, struct epoll_event __user *events, int maxevents,
                  int timeout, const sigset_t __user *sigmask,
                  size_t sigsetsize))
{
    if (sigsetsize != sizeof(k_sigset_t))
    {
        return -EINVAL;
    }

    return vfs_epoll_pwait(epfd, events, maxevents, timeout, (const k_sigset_t *)sigmask);
}
