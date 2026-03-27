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

#include <atomic.h>
#include <errno.h>
#include <time/ktime.h>
#include <time/ktimer.h>
#include <ttos_time.h>

static atomic64_t g_offset_real;
static atomic64_t g_offset_boot;
static atomic64_t g_offset_tai;
static struct timezone g_tz;

ktime_t ktime_now(void)
{
    return ttos_time_walltime_get();
}

ktime_t ktime_get_boottime(void)
{
    return (ktime_now() + atomic64_read(&g_offset_boot));
}

ktime_t ktime_get_clocktai(void)
{
    return (ktime_now() + atomic64_read(&g_offset_tai));
}

ktime_t ktime_get_real(void)
{
    return (ktime_now() + atomic64_read(&g_offset_real));
}

void ktime_set_real(ktime_t offset)
{
    atomic64_add(&g_offset_real, offset);

    /* 通知所有核更新offset,并处理timer队列 */
    ktimer_handle_clock_change();
}

void ktime_get_boottime_ts64(struct timespec64 *ts)
{
    ktime_t ktime = ktime_get_boottime();

    ts->tv_sec = ktime / NSEC_PER_SEC;
    ts->tv_nsec = ktime % NSEC_PER_SEC;
}

void read_realtime_ts64(struct timespec64 *ts)
{
    ktime_t ktime = ktime_get_real();

    ts->tv_sec = ktime / NSEC_PER_SEC;
    ts->tv_nsec = ktime % NSEC_PER_SEC;
}

int ktime_get_monotonic_timespec(clockid_t which_clock, struct timespec64 *tp)
{
    ktime_t ktime = ktime_now();

    tp->tv_sec = ktime / NSEC_PER_SEC;
    tp->tv_nsec = ktime % NSEC_PER_SEC;

    return 0;
}

ktime_t ktime_get_monotonic_ktime(clockid_t which_clock)
{
    return ktime_now();
}

void ktime_get_update_offsets(ktime_t *offs_real, ktime_t *offs_boot, ktime_t *offs_tai)
{
    *offs_real = atomic64_read(&g_offset_real);
    *offs_boot = atomic64_read(&g_offset_boot);
    *offs_tai = atomic64_read(&g_offset_tai);
}

struct timespec64 timespec64_from_ns(s64 nsec)
{
    struct timespec64 ts = {0, 0};
    s32 rem;

    if (likely(nsec > 0))
    {
        ts.tv_sec = nsec / NSEC_PER_SEC;
        ts.tv_nsec = nsec % NSEC_PER_SEC;
    }
    else if (nsec < 0)
    {
        ts.tv_sec = -((-nsec - 1) / NSEC_PER_SEC) - 1;
        rem = (-nsec - 1) % NSEC_PER_SEC;
        ts.tv_nsec = NSEC_PER_SEC - rem - 1;
    }

    return ts;
}

void g_tz_get(struct timezone *tz_prt)
{
    *tz_prt = g_tz;
}

int kernel_settimeofday64(const struct timespec64 *tv, const struct timezone *tz)
{
    ktime_t now = 0;

    if (tv && !timespec64_valid(tv))
    {
        return -EINVAL;
    }

    if (tv && (unsigned long long)tv->tv_sec >= KTIME_SETTIME_SEC_MAX)
    {
        return -EINVAL;
    }

    if (tv)
    {
        ktime_t time = timespec64_to_ktime(*tv);
        now = ktime_get_real();

        ktime_set_real(time - now);
    }

    if (tz)
    {
        if (tz->tz_minuteswest > 15 * 60 || tz->tz_minuteswest < -15 * 60)
        {
            return -EINVAL;
        }

        g_tz = *tz;
    }

    return 0;
}
