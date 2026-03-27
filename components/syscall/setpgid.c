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
#include <ttosProcess.h>

/**
 * @brief 系统调用实现：设置进程组ID。
 *
 * 该函数实现了一个系统调用，用于设置指定进程的进程组ID。
 *
 * @param[in] pid 目标进程ID，如果为0则表示当前进程
 * @param[in] pgid 新的进程组ID，如果为0则使用pid作为进程组ID
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功。
 * @retval -ESRCH 进程不存在。
 * @retval -EPERM 权限不足。
 * @retval -EINVAL 参数无效。
 *
 * @note 1. 需要特权。
 *       2. 影响进程组。
 *       3. 立即生效。
 *       4. 进程级操作。
 */
int setpgid(pid_t pid, pid_t pgid);

DEFINE_SYSCALL (setpgid, (pid_t pid, pid_t pgid))
{
    return setpgid(pid, pgid);
}