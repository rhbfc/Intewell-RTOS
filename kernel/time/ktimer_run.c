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

#include <errno.h>
#include <klog.h>
#include <smp.h>
#include <stdlib.h>
#include <time/ktimer.h>
#include <ttosHal.h>
#include <util.h>

#include "ktimer_internal.h"

#define KTIMER_TRY_TO_CANCEL_NOT_LOCAL (-2)
#define KTIMER_TRY_TO_CANCEL_RUNNING (-3)

struct ktimer_cancel_req
{
    struct ktimer *timer;
    int32_t ret;
};

static bool ktimer_is_local(struct ktimer *timer)
{
    return timer->base->cpu_base == this_cpu_ptr(&ktimer_bases);
}

static bool ktimer_callback_is_running(struct ktimer *timer)
{
    return timer->base->running_timer == timer;
}

static void _ktimer_remove(struct ktimer *timer, struct ktimer_clock_base *base, uint32_t newstate,
                           int32_t reprogram)
{
    struct ktimer_cpu_base *cpu_base = base->cpu_base;
    uint32_t state = (uint32_t)atomic_read(&timer->state);

    atomic_write(&timer->state, newstate);

    if (!(state & KTIMER_ENQUEUED))
        return;

    ktimerqueue_del(&base->queue, &timer->queue_entry);
    if (ktimerqueue_is_empty(&base->queue))
        cpu_base->active_mask &= ~(1U << base->base_type);

    if (reprogram && timer == cpu_base->next_expiring)
        ktimer_refresh_next_deadline(cpu_base);
}

int32_t ktimer_dequeue_timer(struct ktimer *timer, struct ktimer_clock_base *base, bool restart)
{
    bool reprogram;

    if (!ktimer_is_enqueued(timer))
        return 0;

    reprogram = ktimer_is_local(timer);

    /* restart=true 临时移除，后续会重新入队，所以无需重新编程。 */
    if (restart)
        reprogram = false;

    _ktimer_remove(timer, base, KTIMER_INACTIVE, reprogram);
    return 0;
}

int32_t ktimer_queue_timer(struct ktimer *timer, struct ktimer_clock_base *base)
{
    base->cpu_base->active_mask |= 1U << base->base_type;
    atomic_write(&timer->state, KTIMER_ENQUEUED);

    return ktimerqueue_add(&base->queue, &timer->queue_entry);
}

bool ktimer_is_active(struct ktimer *timer)
{
    /* timer 从未调用过 ktimer_start，即 timer 从未启动过。 */
    if (!timer->base)
        return false;

    /* 状态不为 KTIMER_INACTIVE 时，说明 timer 仍在队列中。 */
    if (atomic_read(&timer->state) != KTIMER_INACTIVE)
        return true;

    return ktimer_callback_is_running(timer);
}

static int32_t ktimer_try_to_cancel_local(struct ktimer *timer, struct ktimer_clock_base *base)
{
    irq_flags_t flag;
    int32_t ret = -1;

    ttos_int_lock(flag);
    if (!ktimer_callback_is_running(timer))
        ret = ktimer_dequeue_timer(timer, base, false);
    ttos_int_unlock(flag);

    return ret;
}

static void ktimer_cancel_remote_cb(void *info)
{
    struct ktimer_cancel_req *req = (struct ktimer_cancel_req *)info;
    struct ktimer_clock_base *base;
    irq_flags_t flag;

    ttos_int_lock(flag);

    base = req->timer->base;
    if (!ktimer_is_local(req->timer))
    {
        req->ret = KTIMER_TRY_TO_CANCEL_NOT_LOCAL;
        ttos_int_unlock(flag);
        return;
    }

    if (ktimer_callback_is_running(req->timer))
        req->ret = KTIMER_TRY_TO_CANCEL_RUNNING;
    else
        req->ret = ktimer_dequeue_timer(req->timer, base, false);

    ttos_int_unlock(flag);
}

static int32_t ktimer_try_to_cancel_remote(struct ktimer *timer)
{
    cpu_set_t cpus;
    int32_t ret = -1;
    struct ktimer_cancel_req *req;

    req = calloc(1, sizeof(*req));
    if (!req)
        return -1;

    req->timer = timer;
    req->ret = -1;

    CPU_ZERO(&cpus);
    CPU_SET(timer->base->cpu_base->cpu, &cpus);

    while (ret)
    {
        ret = smp_call_function_sync(&cpus, ktimer_cancel_remote_cb, req);
        if (!ret)
            break;
    }

    ret = req->ret;
    free(req);

    return ret;
}

int32_t ktimer_try_to_cancel(struct ktimer *timer)
{
    struct ktimer_clock_base *base;
    int32_t ret = -1;

    if (!ktimer_is_active(timer))
        return 0;

    TTOS_DisablePreempt();

    base = READ_ONCE(timer->base);
    if (ktimer_is_local(timer))
    {
        ret = ktimer_try_to_cancel_local(timer, base);
        TTOS_EnablePreempt();
    }
    else
    {
        TTOS_EnablePreempt();
        ret = ktimer_try_to_cancel_remote(timer);
    }

    return ret;
}

/* 不允许在回调中取消自己，如果在 timer 的 handler 中想取消自己，直接返回 KTIMER_NORESTART 即可。 */
int32_t ktimer_cancel(struct ktimer *timer)
{
    int32_t ret;
    irq_flags_t flag;

    ttos_int_lock(flag);
    if (ktimer_is_local(timer) && ktimer_callback_is_running(timer))
    {
        ttos_int_unlock(flag);
        printk("%s self in handler is not allowed!!!\n", __func__);
        return -EBUSY;
    }
    ttos_int_unlock(flag);

    while (1)
    {
        ret = ktimer_try_to_cancel(timer);
        if (!ret)
            break;
    }

    return 0;
}

static void ktimer_handler_run(struct ktimer_clock_base *base, struct ktimer *timer)
{
    enum ktimer_restart (*func)(struct ktimer *);
    int32_t restart;

    base->running_timer = timer;
    _ktimer_remove(timer, base, KTIMER_INACTIVE, 0);

    func = timer->function;
    restart = func(timer);
    if (restart != KTIMER_NORESTART && !ktimer_is_enqueued(timer))
        ktimer_queue_timer(timer, base);

    base->running_timer = NULL;
}

static void ktimer_dispatch_expired_base(struct ktimer_clock_base *base, ktime_t now)
{
    struct ktimerqueue_entry *entry;
    ktime_t base_now = ktime_add(now, base->time_offset);

    while ((entry = ktimerqueue_peek(&base->queue)))
    {
        struct ktimer *timer = container_of(entry, struct ktimer, queue_entry);

        if (base_now < ktimer_get_expires(timer))
        {
            break;
        }

        ktimer_handler_run(base, timer);
    }
}

void ktimer_dispatch_expired_timers(struct ktimer_cpu_base *cpu_base, ktime_t now)
{
    uint32_t base_type;

    for (base_type = 0; base_type < KTIMER_MAX_CLOCK_BASES; base_type++)
    {
        if (!(cpu_base->active_mask & (1U << base_type)))
        {
            continue;
        }

        ktimer_dispatch_expired_base(&cpu_base->bases[base_type], now);
    }
}
