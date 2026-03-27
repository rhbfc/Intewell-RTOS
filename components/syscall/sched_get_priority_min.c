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
 * @brief 系统调用实现：获取调度策略的最小优先级。
 *
 * 该函数实现了一个系统调用，用于获取指定调度策略支持的最小优先级值。
 *
 * @param[in] policy 调度策略：
 *                   - SCHED_FIFO：先进先出调度
 *                   - SCHED_RR：时间片轮转调度
 *                   - SCHED_OTHER：其他调度策略
 * @return 成功时返回最小优先级值，失败时返回负值错误码。
 * @retval >=0 最小优先级值。
 * @retval -EINVAL 无效的调度策略。
 *
 * @note 1. 策略相关。
 *       2. 不同策略值不同。
 *       3. 值越小优先级越低。
 *       4. 需要特权。
 */
DEFINE_SYSCALL (sched_get_priority_min, (int policy))
{
    return 1;
}
