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

/**
 * @brief 系统调用实现：控制epoll实例。
 *
 * 该函数实现了一个系统调用，用于控制epoll实例的行为。
 * 可以添加、修改或删除被监视的文件描述符。
 *
 * @param[in] epfd epoll实例的文件描述符
 * @param[in] op 操作类型：
 *              - EPOLL_CTL_ADD: 添加新的文件描述符
 *              - EPOLL_CTL_MOD: 修改已有的文件描述符
 *              - EPOLL_CTL_DEL: 删除文件描述符
 * @param[in] fd 要操作的目标文件描述符
 * @param[in] event 事件结构体：
 *                - events: 要监视的事件掩码
 *                  - EPOLLIN: 可读
 *                  - EPOLLOUT: 可写
 *                  - EPOLLRDHUP: TCP连接对端关闭
 *                  - EPOLLPRI: 紧急数据可读
 *                  - EPOLLERR: 错误
 *                  - EPOLLHUP: 挂起
 *                  - EPOLLET: 边缘触发
 *                  - EPOLLONESHOT: 一次性触发
 *                - data: 用户数据
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 操作成功。
 * @retval -EBADF epfd或fd不是有效的文件描述符。
 * @retval -EINVAL 无效参数。
 * @retval -EPERM 目标文件描述符不支持epoll。
 * @retval -EEXIST 添加已存在的文件描述符。
 * @retval -ENOENT 修改或删除不存在的文件描述符。
 * @retval -ENOMEM 内存不足。
 *
 * @note 1. 对同一个fd的操作不是线程安全的。
 *       2. EPOLL_CTL_DEL时event参数被忽略。
 *       3. 边缘触发模式需要特别注意处理方式。
 *       4. 文件描述符关闭时会自动从epoll实例中移除。
 */
DEFINE_SYSCALL (epoll_ctl,
                (int epfd, int op, int fd, struct epoll_event __user *event))
{
    return vfs_epoll_ctl (epfd, op, fd, event);
}