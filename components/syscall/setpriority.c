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
 * @brief 系统调用实现：设置进程优先级。
 *
 * 该函数实现了一个系统调用，用于设置进程、进程组或用户的优先级。
 *
 * @param[in] which 目标类型：
 *                  - PRIO_PROCESS：进程
 *                  - PRIO_PGRP：进程组
 *                  - PRIO_USER：用户
 * @param[in] who 目标ID，如果为0则表示当前进程、进程组或用户
 * @param[in] niceval 新的优先级值，范围从-20到19
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功。
 * @retval -ESRCH 目标不存在。
 * @retval -EPERM 权限不足。
 * @retval -EINVAL 参数无效。
 *
 * @note 1. 需要特权。
 *       2. 影响调度。
 *       3. 立即生效。
 *       4. 支持多种目标。
 */
DEFINE_SYSCALL (setpriority, (int which, int who, int niceval))
{
    /* 没有nice机制 恒返回0 */
    return 0;
}
