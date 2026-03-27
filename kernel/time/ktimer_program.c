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

#include <klog.h>
#include <percpu.h>
#include <ttos_time.h>

#include "ktimer_internal.h"

/* 某些硬件在设置一个过去时间时，不会立刻产生中断；这里预留一个最小前瞻量做兼容。设为0表示关闭该兼容。 */
#ifndef CONFIG_KTIMER_PAST_DEADLINE_GUARD_NS
#define CONFIG_KTIMER_PAST_DEADLINE_GUARD_NS 2000
#endif

int32_t ktimer_arm_next_deadline(ktime_t next_ns, struct ktimer_cpu_base *cpu_base)
{
    int32_t ret = 0;
    ktime_t now;

    cpu_base->next_deadline = next_ns;
    now = ktime_get_ns();

    if (next_ns < now)
    {
        ret = 1;
    }

#if CONFIG_KTIMER_PAST_DEADLINE_GUARD_NS > 0
    /* 硬件不支持“过去时间立即触发”时，把到期时间轻微推后，确保能真正编程成功。 */
    if (ret)
    {
        next_ns = now + CONFIG_KTIMER_PAST_DEADLINE_GUARD_NS;
    }
#endif

    ttos_time_timeout_ns_set(next_ns);

    return ret;
}

int32_t ktimer_refresh_next_deadline(struct ktimer_cpu_base *cpu_base)
{
    ktime_t next = ktimer_select_next_deadline(cpu_base);

    return ktimer_arm_next_deadline(next, cpu_base);
}

void ktimer_commit_next_deadline(struct ktimer_cpu_base *cpu_base, ktime_t expires_next)
{
    cpu_base->next_deadline = expires_next;
    ttos_time_timeout_ns_set(expires_next);
}

void ktimer_reschedule_timer(struct ktimer *timer)
{
    struct ktimer_cpu_base *cpu_base = this_cpu_ptr(&ktimer_bases);
    struct ktimer_clock_base *base = timer->base;
    ktime_t expires = ktime_sub(ktimer_get_expires(timer), base->time_offset);

    /* 由于存在 time_offset, 所以 expires 可能小于 0 */
    if (expires < 0)
        expires = 0;

    if (base->cpu_base != cpu_base)
    {
        printk("fail at %s:%d\n", __FILE__, __LINE__);
        return;
    }

    if (expires >= cpu_base->next_deadline)
        return;

    /* 由于 ktimer 中断 handler 处理完后，会重新评估下一次的中断，所以直接返回即可 */
    if (cpu_base->in_irq)
        return;

    cpu_base->next_expiring = timer;
    ktimer_commit_next_deadline(cpu_base, expires);
}
