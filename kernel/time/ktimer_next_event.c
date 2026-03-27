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

#include <time/ktimer.h>
#include <util.h>

#include "ktimer_internal.h"

static struct ktimer *ktimer_peek_earliest_timer(struct ktimer_clock_base *base)
{
    struct ktimerqueue_entry *entry;

    entry = ktimerqueue_peek(&base->queue);
    if (!entry)
    {
        return NULL;
    }

    return container_of(entry, struct ktimer, queue_entry);
}

static ktime_t ktimer_find_next_deadline(struct ktimer_cpu_base *cpu_base)
{
    uint32_t base_type;
    ktime_t next_deadline = KTIME_MAX;

    /* 遍历当前 CPU 的所有活跃 base，找出最早到期的 timer */
    for (base_type = 0; base_type < KTIMER_MAX_CLOCK_BASES; base_type++)
    {
        struct ktimer_clock_base *base;
        struct ktimer *timer;
        ktime_t deadline;

        if (!(cpu_base->active_mask & (1U << base_type)))
        {
            continue;
        }

        base = &cpu_base->bases[base_type];
        timer = ktimer_peek_earliest_timer(base);
        if (!timer)
        {
            continue;
        }

        deadline = ktime_sub(ktimer_get_expires(timer), base->time_offset);
        if (deadline < next_deadline)
        {
            next_deadline = deadline;
            cpu_base->next_expiring = timer;
        }
    }

    /* clock_was_set 可能改变 base->time_offset，结果可能为负，这里修正 */
    if (next_deadline < 0)
    {
        next_deadline = 0;
    }

    return next_deadline;
}

ktime_t ktimer_select_next_deadline(struct ktimer_cpu_base *cpu_base)
{
    cpu_base->next_expiring = NULL;

    return ktimer_find_next_deadline(cpu_base);
}
