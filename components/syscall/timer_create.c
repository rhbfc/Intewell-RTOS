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
#include <time/posix_timer.h>
#include <time/ktime.h>
#include <uaccess.h>
#include <errno.h>
#include <time.h>
#include <bits/alltypes.h>
#include <ttosProcess.h>

/**
 * @brief 内部函数：创建一个POSIX定时器。
 *
 * 该函数实现了一个内部函数，用于创建一个新的POSIX定时器。
 * POSIX定时器提供了比传统interval timer更灵活的定时机制，
 * 支持多种时钟源和通知方式。
 *
 * @param[in] which_clock 时钟源标识符：
 *                  - CLOCK_REALTIME: 系统实时时钟
 *                  - CLOCK_MONOTONIC: 单调递增时钟
 *                  - CLOCK_PROCESS_CPUTIME_ID: 进程CPU时间
 *                  - CLOCK_THREAD_CPUTIME_ID: 线程CPU时间
 * @param[in] event 定时器通知事件结构体：
 *                - sigev_notify: 通知方式（SIGEV_SIGNAL等）
 *                - sigev_signo: 要发送的信号
 *                - sigev_value: 信号附带的数据
 *                - NULL: 使用默认设置（SIGALRM信号）
 * @param[out] created_timer_id 定时器ID的指针，用于存储新创建的定时器ID
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 操作成功。
 * @retval -EAGAIN 系统资源不足。
 * @retval -EINVAL which_clock无效或event包含无效值。
 * @retval -EFAULT created_timer_id或event指向无效内存。
 * @retval -ENOTSUP 不支持指定的时钟源或通知方式。
 *
 * @note 1. 新创建的定时器默认是停止状态，需要通过timer_settime()启动。
 *       2. 每个进程可以创建的定时器数量有限制。
 *       3. 进程终止时，其所有定时器都会被自动删除。
 */

/**
 * @brief 系统调用：创建一个POSIX定时器。
 *
 * 该函数实现了一个系统调用，用于创建一个新的POSIX定时器。
 * POSIX定时器提供了比传统interval timer更灵活的定时机制，
 * 支持多种时钟源和通知方式。
 *
 * @param[in] which_clock 时钟源标识符：
 *                  - CLOCK_REALTIME: 系统实时时钟
 *                  - CLOCK_MONOTONIC: 单调递增时钟
 *                  - CLOCK_PROCESS_CPUTIME_ID: 进程CPU时间
 *                  - CLOCK_THREAD_CPUTIME_ID: 线程CPU时间
 * @param[in] timer_event_spec 定时器通知事件结构体：
 *                - sigev_notify: 通知方式（SIGEV_SIGNAL等）
 *                - sigev_signo: 要发送的信号
 *                - sigev_value: 信号附带的数据
 *                - NULL: 使用默认设置（SIGALRM信号）
 * @param[out] created_timer_id 定时器ID的指针，用于存储新创建的定时器ID
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 操作成功。
 * @retval -EAGAIN 系统资源不足。
 * @retval -EINVAL which_clock无效或timer_event_spec包含无效值。
 * @retval -EFAULT created_timer_id或timer_event_spec指向无效内存。
 * @retval -ENOTSUP 不支持指定的时钟源或通知方式。
 *
 * @note 1. 新创建的定时器默认是停止状态，需要通过timer_settime()启动。
 *       2. 每个进程可以创建的定时器数量有限制。
 *       3. 进程终止时，其所有定时器都会被自动删除。
 */
DEFINE_SYSCALL(timer_create, (clockid_t which_clock,
            struct sigevent __user *timer_event_spec,
            timer_t __user *created_timer_id))
{

    if (timer_event_spec)
    {
        sigevent_t event;

        if (copy_from_user(&event, timer_event_spec, sizeof(event)))
        {
            return -EFAULT;
        }

        return posix_timer_create(which_clock, &event, created_timer_id);
    }

    return posix_timer_create(which_clock, NULL, created_timer_id);
}
