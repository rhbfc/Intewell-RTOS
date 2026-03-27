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

extern int timer_overrun_to_int(struct posix_timer *posix_timer);

/**
 * @brief 系统调用实现：获取POSIX定时器的溢出次数。
 *
 * 该函数实现了一个系统调用，用于获取指定POSIX定时器在最近一次
 * 定时器到期通知被传递后，又发生了多少次到期。这用于检测由于
 * 系统负载过重而错过的定时器到期事件。
 *
 * @param[in] timerid 定时器ID
 * @return 成功时返回溢出计数，失败时返回负值错误码。
 * @retval >=0 溢出计数。
 * @retval -EINVAL timerid无效。
 *
 * @note 1. 溢出计数最大为DELAYTIMER_MAX。
 *       2. 如果溢出计数超过DELAYTIMER_MAX，返回DELAYTIMER_MAX。
 *       3. 每次成功传递定时器到期通知后，溢出计数会被重置为0。
 */
DEFINE_SYSCALL(timer_getoverrun, (timer_t timer_id))
{
    int ret;
    struct posix_timer *timer;

    timer = posix_timer_get((int)(uintptr_t)timer_id);
    if (!timer)
    {
        return -EINVAL;
    }

    if (timer->pcb->group_leader != ttosProcessSelf()->group_leader)
    {
        posix_timer_putref(timer);
        return -EINVAL;
    }

    ret = timer_overrun_to_int(timer);
    posix_timer_putref(timer);
    return ret;
}
