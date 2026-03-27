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
 * @brief 系统调用实现：设置进程附加组ID列表（32位）。
 *
 * 该函数实现了一个系统调用，用于设置进程的附加组ID列表，支持32位值。
 *
 * @param[in] gidsetsize 组ID列表的大小
 * @param[in] grouplist 组ID列表的指针（32位）
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功。
 * @retval -EPERM 权限不足。
 * @retval -EFAULT 内存访问错误。
 * @retval -EINVAL 参数无效。
 *
 * @note 1. 需要特权。
 *       2. 影响进程权限。
 *       3. 立即生效。
 *       4. 进程级操作。
 */
DEFINE_SYSCALL (setgroups32, (int gidsetsize, gid_t __user *grouplist))
{
    /* 没有多用户 恒返回0 */
    return 0;
}