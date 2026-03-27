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
 * @brief 系统调用实现：获取进程优先级。
 *
 * 该函数实现了一个系统调用，用于获取指定进程的优先级。
 *
 * @param[in] which 指定要获取优先级的目标类型
 * @param[in] who 目标ID
 * @return 成功时返回优先级值，失败时返回负值错误码。
 * @retval >=0 成功，返回优先级值。
 * @retval -EINVAL which参数无效。
 * @retval -ESRCH 指定的进程不存在。
 * @retval -EPERM 没有权限访问指定进程。
 *
 * @note 1. 优先级范围从-20到19。
 *       2. 较低的值表示更高的优先级。
 *       3. 普通用户只能降低优先级。
 *       4. which参数可以是PRIO_PROCESS、PRIO_PGRP或PRIO_USER。
 */
DEFINE_SYSCALL (getpriority, (int which, int who))
{
    /* 没有nice机制 恒返回0 */
    return 0;
}
