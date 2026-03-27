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

#include <clock/clockchip.h>
#include <driver/cpudev.h>
#include <smp.h>
#include <time/ktimer.h>
#include <ttosHal.h>
#include <klog.h>

#include "ktimer_internal.h"

void vdso_update_data(void);

DEFINE_PER_CPU(struct ktimer_cpu_base, ktimer_bases) = {
    .bases =
        {
            {
                .base_type = KTIMER_BASE_MONOTONIC,
                .clock_id = CLOCK_MONOTONIC,
                .read_now = &ktime_now,
            },
            {
                .base_type = KTIMER_BASE_REALTIME,
                .clock_id = CLOCK_REALTIME,
                .read_now = &ktime_get_real,
            },
            {
                .base_type = KTIMER_BASE_BOOTTIME,
                .clock_id = CLOCK_BOOTTIME,
                .read_now = &ktime_get_boottime,
            },
            {
                .base_type = KTIMER_BASE_TAI,
                .clock_id = CLOCK_TAI,
                .read_now = &ktime_get_clocktai,
            },
        },
};

static inline void ktimer_refresh_time_offsets(struct ktimer_cpu_base *base)
{
    ktime_t *offs_real = &base->bases[KTIMER_BASE_REALTIME].time_offset;
    ktime_t *offs_boot = &base->bases[KTIMER_BASE_BOOTTIME].time_offset;
    ktime_t *offs_tai  = &base->bases[KTIMER_BASE_TAI].time_offset;

    ktime_get_update_offsets(offs_real, offs_boot, offs_tai);
}

static void ktimer_notify_clock_change_cpu(void *info)
{
    irq_flags_t flag;

    (void)info;

    ttos_int_lock(flag);
    ktimer_interrupt(-1, NULL);
    ttos_int_unlock(flag);
}

void ktimer_handle_clock_change(void)
{
    cpu_set_t cpuset;
    struct cpudev *cpu;

    vdso_update_data();
    CPU_ZERO(&cpuset);

    for_each_present_cpu(cpu)
    {
        if (CPU_STATE_RUNNING != cpu->state)
            continue;

        CPU_SET(cpu->index, &cpuset);
    }

    smp_call_function_async(&cpuset, ktimer_notify_clock_change_cpu, NULL);
}

static void ktimer_recover_overrun(ktime_t entry_time, struct ktimer_cpu_base *cpu_base)
{
	ktime_t delta;
	ktime_t expires_next;
	ktime_t force_next_interval;

	ktime_t now = ktime_now();

	/* 计算本次handler所消耗的时间 */
	delta = ktime_sub(now, entry_time);

    expires_next = ktime_add(now, delta);
    force_next_interval = delta;

	ktimer_arm_next_deadline(expires_next, cpu_base);

	KLOG_W("ktimer: overrun, handler used %llu ns, force next interval %llu ns", delta, force_next_interval);
}

/* 在关中断情况下调用 */
void ktimer_interrupt(uint32_t irq, void *arg)
{
    int32_t overrun = 0;
    struct clockchip *cc = (struct clockchip *)arg;
    struct ktimer_cpu_base *cpu_base = this_cpu_ptr(&ktimer_bases);
    ktime_t expires_next;
    ktime_t now;

    (void)irq;

    if (cc && cc->bound_on != cpuid_get())
    {
        return;
    }

    /* 如果在进入中断过程中，其它核修改了时间偏移，本核也能立刻感知到并生效。 */
    ktimer_refresh_time_offsets(cpu_base);

retry:

    now = ktime_now();

    cpu_base->in_irq = 1;

    ktimer_dispatch_expired_timers(cpu_base, now);

    expires_next = ktimer_select_next_deadline(cpu_base);
    cpu_base->next_deadline = expires_next;

    cpu_base->in_irq = 0;

    if (!ktimer_arm_next_deadline(expires_next, cpu_base))
	{
		/* 编程下一次成功，返回 */
		return;
	}

	/* 编程下一次失败(由于要设置的时间已经是过期时间) */

	/* 可能有offset已经更新 */
	ktimer_refresh_time_offsets(cpu_base);

    overrun ++;

	/* 如果要设置的时间已经是过去时间，则再一次尝试处理 */
	if (overrun < 5)
	{
		goto retry;
	}

	/* 正常情况下不会执行到此, 连续5次仍是过期时间：认为本次中断处理超时，触发overrun处理并重新编程下一次 */
	ktimer_recover_overrun(now, cpu_base);
}

/* 每个核初始化时调用 */
void ktimer_sys_init(void)
{
    struct ktimer_cpu_base *cpu_base = this_cpu_ptr(&ktimer_bases);
    int32_t i;

    cpu_base->cpu = cpuid_get();
    cpu_base->active_mask = 0;
    cpu_base->in_irq = 0;
    cpu_base->next_deadline = KTIME_MAX;
    cpu_base->next_expiring = NULL;

    for (i = 0; i < KTIMER_MAX_CLOCK_BASES; i++)
    {
        struct ktimer_clock_base *base = &cpu_base->bases[i];

        base->cpu_base = cpu_base;
        base->running_timer = NULL;
        base->time_offset = 0;
        ktimerqueue_init(&base->queue);
    }

    ktimer_refresh_time_offsets(cpu_base);
}
