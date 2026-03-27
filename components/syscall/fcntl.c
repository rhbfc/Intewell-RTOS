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
#include <fcntl.h>

/**
 * @brief 系统调用实现：文件控制操作。
 *
 * 该函数实现了一个系统调用，用于对打开的文件描述符进行各种控制操作。
 *
 * @param[in] fd 文件描述符
 * @param[in] cmd 控制命令：
 *                - F_DUPFD: 复制文件描述符
 *                - F_GETFD: 获取文件描述符标志
 *                - F_SETFD: 设置文件描述符标志
 *                - F_GETFL: 获取文件状态标志
 *                - F_SETFL: 设置文件状态标志
 *                - F_GETLK: 获取记录锁信息
 *                - F_SETLK: 设置记录锁
 *                - F_SETLKW: 设置记录锁(等待)
 * @param[in] arg 命令参数
 * @return 成功时返回值依赖于cmd，失败时返回负值错误码。
 * @retval >=0 成功(具体值依赖于cmd)。
 * @retval -EBADF fd不是有效的文件描述符。
 * @retval -EINVAL 无效的cmd或arg。
 * @retval -EMFILE 进程已达到文件描述符限制。
 * @retval -EACCES 访问被拒绝。
 * @retval -EAGAIN 资源暂时不可用。
 *
 * @note 1. F_DUPFD返回新的文件描述符。
 *       2. F_GETFD和F_GETFL返回标志值。
 *       3. F_SETFD和F_SETFL返回0表示成功。
 *       4. 锁相关操作可能会阻塞。
 */
DEFINE_SYSCALL (fcntl, (int fd, unsigned int cmd, unsigned long arg))
{
    return vfs_fcntl (fd, cmd, arg);
}

__alias_syscall (fcntl, fcntl64);
