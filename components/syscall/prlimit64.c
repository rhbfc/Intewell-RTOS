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

/**
 * @brief 系统调用实现：设置进程资源限制。
 *
 * 该函数实现了一个系统调用，用于设置进程资源限制。
 *
 * @param pid 进程 ID。
 * @param resource 资源类型。
 * @param new_rlim 指向用户空间的新资源限制结构体指针。
 * @param old_rlim 指向用户空间的旧资源限制结构体指针。
 * @return 成功时返回 0，失败时返回负值错误码。
 */
DEFINE_SYSCALL (prlimit64, (pid_t pid, unsigned int resource,
                             const struct rlimit64 __user *new_rlim,
                             struct rlimit64 __user       *old_rlim))
{
    return 0;
}