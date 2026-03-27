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
#include <fs/kpoll.h>
#include <ttos.h>
#include <ttosProcess.h>

/**
 * @brief 系统调用实现：等待一组文件描述符上的事件。
 *
 * 该函数实现了一个系统调用，用于监视多个文件描述符，等待其中的一个或多个
 * 文件描述符成为"就绪"状态以执行某些I/O操作。
 *
 * @param[in,out] ufds pollfd结构数组，每个结构包含：
 *                    - fd: 要监视的文件描述符
 *                    - events: 要监视的事件掩码
 *                    - revents: 返回时实际发生的事件
 * @param[in] nfds 数组ufds中的元素个数。
 * @param[in] timeout_msecs 超时时间（毫秒）：
 *                         - -1: 永远等待
 *                         - 0: 立即返回
 *                         - >0: 等待指定的毫秒数
 * @return 成功时返回就绪描述符的数量，失败时返回负值错误码。
 * @retval >0 有事件发生的文件描述符数量。
 * @retval 0 超时。
 * @retval -EFAULT ufds指向无效内存。
 * @retval -EINTR 被信号中断。
 * @retval -EINVAL nfds超过系统限制。
 * @retval -ENOMEM 内存不足。
 */
DEFINE_SYSCALL(poll, (struct pollfd __user * ufds, unsigned int nfds, int timeout))
{
    struct kpollfd *kfds;
    int i;
    int ret;

    if (ufds == NULL || nfds == 0)
    {
        return -EINVAL;
    }
    kfds = calloc(nfds, sizeof(struct kpollfd));
    if (kfds == NULL)
    {
        return -ENOMEM;
    }
    for (i = 0; i < nfds; i++)
    {
        kfds[i].pollfd = ufds[i];
    }

    ret = kpoll(kfds, nfds, timeout < 0 ? TTOS_WAIT_FOREVER : timeout);

    for (i = 0; i < nfds; i++)
    {
        ufds[i] = kfds[i].pollfd;
    }
    free(kfds);
    return ret;
}
