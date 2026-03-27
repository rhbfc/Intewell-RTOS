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
#include <uaccess.h>

/**
 * @brief 系统调用实现：向指定线程发送信号。
 *
 * 该函数实现了一个系统调用，用于向指定线程ID的线程发送信号。
 *
 * @param[in] tid 目标线程ID
 * @param[in] sig 要发送的信号
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功发送信号。
 * @retval -ESRCH 指定的线程不存在。
 * @retval -EINVAL 无效的信号。
 * @retval -EPERM 权限不足。
 *
 * @note 1. 与kill不同，tkill只能向指定线程发送信号。
 *       2. 需要适当的权限。
 *       3. 线程必须存在且可被发送信号。
 *       4. 信号必须是有效的。
 */
DEFINE_SYSCALL(tkill, (pid_t tid, int sig))
{
    return kernel_signal_send(tid, TO_THREAD, sig, SI_TKILL, NULL);
}
