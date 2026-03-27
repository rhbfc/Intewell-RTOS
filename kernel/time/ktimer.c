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
#include <klog.h>
#include <time/ktimer.h>
#include <ttos_time.h>
#include <ttosHal.h>
#include <util.h>
#include <errno.h>

#include "ktimer_internal.h"

#define HIGH_RES_NSEC 1

static uint64_t ktimer_resolution = HIGH_RES_NSEC;
static const int32_t ktimer_clock_id_to_base_type_map[CLOCK_TAI + 1] = {
    [0 ... CLOCK_TAI] = -1,
    [CLOCK_REALTIME] = KTIMER_BASE_REALTIME,
    [CLOCK_MONOTONIC] = KTIMER_BASE_MONOTONIC,
    [CLOCK_BOOTTIME] = KTIMER_BASE_BOOTTIME,
    [CLOCK_TAI] = KTIMER_BASE_TAI,
};

/* ==================== 内部前置声明 ==================== */

static struct ktimer_clock_base *ktimer_resolve_base(struct ktimer *timer);

/* ==================== 分辨率与时钟基址 ==================== */

uint64_t ktimer_get_resolution_ns(void)
{
    return ktimer_resolution;
}

int32_t ktimer_set_resolution(uint64_t freq)
{
    uint64_t nsec;

    if (freq == 0)
    {
        return -EINVAL;
    }

    nsec = NSEC_PER_SEC / freq;
    if (nsec == 0)
    {
        nsec = 1;
    }

    ktimer_resolution = nsec;
    return 0;
}

/* 安全相加两个ktime值；一旦检测到溢出，返回可表示的最大ktime。 */
ktime_t ktime_add_safe(const ktime_t kt1, const ktime_t kt2)
{
    ktime_t time = ktime_add_unsafe(kt1, kt2);

    if (time < 0 || time < kt1 || time < kt2)
    {
        time = ktime_set(KTIME_SEC_MAX, 0);
    }

    return time;
}

ktime_t ktimer_get_expires(const struct ktimer *timer)
{
    return timer->queue_entry.expires;
}

ktime_t ktimer_expires_remaining(const struct ktimer *timer)
{
    return ktime_sub(timer->queue_entry.expires, timer->base->read_now());
}

void ktimer_set_expires(struct ktimer *timer, ktime_t time)
{
    timer->queue_entry.expires = time;
}

void ktimer_add_expires(struct ktimer *timer, ktime_t time)
{
    timer->queue_entry.expires = ktime_add_safe(timer->queue_entry.expires, time);
}

ktime_t ktimer_remaining(const struct ktimer *timer)
{
    if (!timer || !timer->base || !timer->base->read_now)
    {
        return 0;
    }

    return ktime_sub(timer->queue_entry.expires, timer->base->read_now());
}

ktime_t ktimer_get_time(struct ktimer *timer)
{
    return timer->base->read_now();
}

void ktimer_start_expires(struct ktimer *timer, enum ktimer_mode mode)
{
    ktime_t expires = ktimer_get_expires(timer);

    ktimer_start(timer, expires, mode);
}

/* 把一个已经过期的定时器，推进到now之后的第一个合法到期点，并返回:推进过程中跨过了多少个周期*/
uint64_t ktimer_forward(struct ktimer *timer, ktime_t now, ktime_t interval)
{
    int64_t interval_ns = 0;
    ktime_t delta;
    uint64_t overrun = 1; /* 大多数情况下,下一个周期不会超期,所以，默认会推进一个周期 */

    /* timer在队列中(正常情况不会出现)*/
    if (ktimer_is_enqueued(timer))
    {
        return 0;
    }

    /* 计算从上次触发到现在的时间差值 */
    delta = ktime_sub(now, ktimer_get_expires(timer));
    if (delta < 0)
    {
        /* timer还未触发 */
        return 0;
    }

    /* 间隔值小于精度,则强制设置为精度时间 */
    if (interval < ktimer_resolution)
    {
        interval = ktimer_resolution;
    }

    /* 从上次触发到现在的时间差值已经大于间隔值，说明已经出现overrun */
    if (unlikely(delta >= interval))
    {
        interval_ns = ktime_to_ns(interval);

        /* 计算overrun次数 */
        overrun = delta / interval_ns;

        /* 设置timer的时间跳过overrun的时间 */
        ktimer_add_expires(timer, interval_ns * overrun);

        /* 设置过后的时间是未来时间 */
        if (ktimer_get_expires(timer) > now)
        {
            return overrun;
        }

        /* 设置过后的时间是过去时间，下边会再增加一个interval,正常情况下就会是未来时间 */
        overrun++;
    }

    ktimer_add_expires(timer, interval);

    return overrun;
}

uint64_t ktimer_forward_now(struct ktimer *timer, ktime_t interval)
{
    return ktimer_forward(timer, ktimer_get_time(timer), interval);
}

void ktimer_restart(struct ktimer *timer)
{
    ktimer_start_expires(timer, KTIMER_MODE_ABS);
}

static inline void ktimer_set_expires_ns(struct ktimer *timer, ktime_t time)
{
    timer->queue_entry.expires = time;
}

static inline int32_t ktimer_clock_id_to_base_type(clockid_t clock_id)
{
    if (clock_id >= 0 && clock_id <= CLOCK_TAI &&
        ktimer_clock_id_to_base_type_map[clock_id] >= 0)
    {
        return ktimer_clock_id_to_base_type_map[clock_id];
    }

    printk("Invalid clockid %d. Using MONOTONIC\n", clock_id);
    return KTIMER_BASE_MONOTONIC;
}

static inline void ktimer_bind_base(struct ktimer *timer, int32_t base_type)
{
    struct ktimer_cpu_base *cpu_base = raw_cpu_ptr(&ktimer_bases);

    timer->base = &cpu_base->bases[base_type];
}

/* ==================== 定时器生命周期 ==================== */

int32_t ktimer_init(struct ktimer *timer, enum ktimer_restart (*function)(struct ktimer *),
                    clockid_t clock_id, enum ktimer_mode mode)
{
    if (!timer || !function)
    {
        return -EINVAL;
    }

    memset(timer, 0, sizeof(struct ktimer));

    if (clock_id == CLOCK_REALTIME && mode & KTIMER_MODE_REL)
    {
        clock_id = CLOCK_MONOTONIC;
    }

    ktimerqueue_entry_init(&timer->queue_entry);

    timer->clock_id = clock_id;
    timer->function = function;

    ktimer_resolve_base(timer);

    atomic_write(&timer->state, KTIMER_INACTIVE);

    return 0;
}

bool ktimer_is_enqueued(struct ktimer *timer)
{
    return (atomic_read(&timer->state) == KTIMER_ENQUEUED) ? true : false;
}

bool ktimer_is_inactive(struct ktimer *timer)
{
    return (atomic_read(&timer->state) == KTIMER_INACTIVE) ? true : false;
}

static bool ktimer_start_on_base(struct ktimer *timer, ktime_t time, enum ktimer_mode mode,
                                 struct ktimer_clock_base *base)
{
    ktimer_dequeue_timer(timer, base, true);

    if (mode & KTIMER_MODE_REL)
    {
        time = ktime_add_safe(time, base->read_now());
    }
    else
    {
        ktime_t now = ktime_now();
        if (now > time)
        {
            KLOG_W("%s time is old dest s:%llu ns:%llu  now s:%llu ns:%llu", __func__,
                   time / NANOSECOND_PER_SECOND, time % NANOSECOND_PER_SECOND,
                   now / NANOSECOND_PER_SECOND, now % NANOSECOND_PER_SECOND);
        }
    }

    ktimer_set_expires_ns(timer, time);

    return ktimer_queue_timer(timer, base);
}

static struct ktimer_clock_base *ktimer_resolve_base(struct ktimer *timer)
{
    int32_t base_type = ktimer_clock_id_to_base_type(timer->clock_id);

    ktimer_bind_base(timer, base_type);

    return timer->base;
}

void ktimer_start(struct ktimer *timer, ktime_t time, const enum ktimer_mode mode)
{
    bool is_first_node = 0;
    irq_flags_t flag;
    struct ktimer_clock_base *base;

    if (!timer || !timer->function)
    {
        printk("%s:timer->function is NULL!\n", __func__);
        return;
    }

    ttos_int_lock(flag);

    /* 强制绑定ktimer到当前核 */
    base = ktimer_resolve_base(timer);

    /* 启动ktimer，并返回该timer是否是第一个节点,即最先到期的节点 */
    is_first_node = ktimer_start_on_base(timer, time, mode, base);
    if (is_first_node)
    {
        /* 第一个节点,则重新设置硬件 */
        ktimer_reschedule_timer(timer);
    }

    ttos_int_unlock(flag);
}

