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
#include <unistd.h>

/**
 * @brief 系统调用实现：获取会话ID。
 *
 * 该函数实现了一个系统调用，用于获取指定进程的会话ID。
 *
 * @param[in] pid 进程ID，如果为0则获取调用进程的会话ID
 * @return 成功时返回会话ID，失败时返回负值错误码。
 * @retval >0 成功，返回会话ID。
 * @retval -ESRCH 指定的进程不存在。
 * @retval -EPERM 没有权限访问指定进程。
 *
 * @note 1. 会话ID等于会话首进程的进程ID。
 *       2. 会话用于终端作业控制。
 *       3. 一个会话可以包含多个进程组。
 *       4. 会话可以关联一个控制终端。
 */
DEFINE_SYSCALL (getsid, (pid_t pid))
{
    return SYSCALL_FUNC (getpgid) (pid);
}