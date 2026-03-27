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

#ifndef __KTIMER_INTERNAL_H
#define __KTIMER_INTERNAL_H

#include <percpu.h>
#include <time/ktimer.h>

DECLARE_PER_CPU(struct ktimer_cpu_base, ktimer_bases);

ktime_t ktimer_select_next_deadline(struct ktimer_cpu_base *cpu_base);
int32_t ktimer_arm_next_deadline(ktime_t next_ns, struct ktimer_cpu_base *cpu_base);
int32_t ktimer_refresh_next_deadline(struct ktimer_cpu_base *cpu_base);
void ktimer_commit_next_deadline(struct ktimer_cpu_base *cpu_base, ktime_t expires_next);
void ktimer_reschedule_timer(struct ktimer *timer);
int32_t ktimer_queue_timer(struct ktimer *timer, struct ktimer_clock_base *base);
int32_t ktimer_dequeue_timer(struct ktimer *timer, struct ktimer_clock_base *base, bool restart);
void ktimer_dispatch_expired_timers(struct ktimer_cpu_base *cpu_base, ktime_t now);

#endif /* __KTIMER_INTERNAL_H */
