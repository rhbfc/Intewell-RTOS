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

#ifndef __K_TIME__
#define __K_TIME__

#include <list.h>
#include <spinlock.h>
#include <stdbool.h>
#include <sys/time.h>
#include <system/types.h>
#include <time.h>

#define __NEED_clock_t
#include <bits/alltypes.h>

#ifndef __user
#define __user
#endif

#define TTOS_TICK_PER_SEC TTOS_GetSysClkRate()

#define MSEC_PER_SEC (1000ULL)
#define USEC_PER_MSEC (1000ULL)
#define NSEC_PER_USEC (1000ULL)
#define NSEC_PER_MSEC (1000000ULL)
#define USEC_PER_SEC (1000000ULL)
#define NSEC_PER_SEC (1000000000ULL)
#define FSEC_PER_SEC (1000000000000000ULL)

#define MUSL_SC_CLK_TCK 100

#define ktime_t int64_t

struct posix_clock_ops;
struct timespec;
struct timespec64;
struct itimerspec64;
typedef struct T_TTOS_ProcessControlBlock *pcb_t;

#define TIME64_MAX ((s64) ~((u64)1 << 63))
#define TIME64_MIN (-TIME64_MAX - 1)

#define KTIME_MAX ((s64) ~((u64)1 << 63))
#define KTIME_MIN (-KTIME_MAX - 1)
#define KTIME_SEC_MAX (KTIME_MAX / NSEC_PER_SEC)
#define KTIME_SEC_MIN (KTIME_MIN / NSEC_PER_SEC)

#define KTIME_UPTIME_HEADROOM_SEC (30LL * 365 * 24 * 3600)
#define KTIME_SETTIME_SEC_MAX (KTIME_SEC_MAX - KTIME_UPTIME_HEADROOM_SEC)

static inline bool timespec64_valid(const struct timespec64 *ts)
{
    if (ts->tv_sec < 0)
        return false;

    if ((unsigned long)ts->tv_nsec >= NSEC_PER_SEC)
        return false;
    return true;
}

static inline s64 timespec64_to_ns(const struct timespec64 *ts)
{
    if (ts->tv_sec >= KTIME_SEC_MAX)
        return KTIME_MAX;

    return ((s64)ts->tv_sec * NSEC_PER_SEC) + ts->tv_nsec;
}

static inline s64 timespec_to_ns(const struct timespec *ts)
{
    if (ts->tv_sec >= KTIME_SEC_MAX)
        return KTIME_MAX;

    return (ts->tv_sec * NSEC_PER_SEC) + ts->tv_nsec;
}

/* raw ktime sources */
ktime_t ktime_now(void);
ktime_t ktime_get_real(void);
ktime_t ktime_get_boottime(void);
ktime_t ktime_get_clocktai(void);
ktime_t ktime_get_monotonic_ktime(clockid_t which_clock);

/* ktime offset management */
void ktime_set_real(ktime_t offset);
void ktime_get_update_offsets_now(ktime_t *offs_real, ktime_t *offs_boot, ktime_t *offs_tai);
void ktime_get_update_offsets(ktime_t *offs_real, ktime_t *offs_boot, ktime_t *offs_tai);

#define ktime_get_ns ktime_now
#define ktime_get_real_ns ktime_get_real
#define ktime_get_boottime_ns ktime_get_boottime
#define ktime_get_clocktai_ns ktime_get_clocktai

#define ktime_sub(kt1, kt2) ((kt1) - (kt2))
#define ktime_add(kt1, kt2) ((kt1) + (kt2))
#define ktime_add_ns(ktime, ns) ((ktime) + (ns))
#define ktime_add_unsafe(kt1, kt2) ((u64)(kt1) + (kt2))

static inline ktime_t ktime_set(const s64 secs, const unsigned long nsecs)
{
    if (unlikely(secs >= KTIME_SEC_MAX))
        return KTIME_MAX;

    return secs * NSEC_PER_SEC + (s64)nsecs;
}

static inline s64 ktime_to_ns(const ktime_t ktime)
{
    return ktime;
}

static inline ktime_t timespec64_to_ktime(struct timespec64 ts)
{
    return ktime_set(ts.tv_sec, ts.tv_nsec);
}

#define ktime_to_timespec64(ktime) timespec64_from_ns((ktime))

/* kernel clock/time entry points */
int kernel_clock_settime(clockid_t clock_id, const struct timespec64 *tp);
int kernel_clock_gettime(clockid_t clk, struct timespec64 *ts);
int kernel_clock_getres(clockid_t clockid, struct timespec64 *res);
int kernel_gettimeofday(struct timeval *tp, struct timezone *tzp);
int kernel_settimeofday(const struct timeval *tp, const struct timezone *tzp);
int kernel_settimeofday64(const struct timespec64 *tv, const struct timezone *tz);
time_t kernel_time(void);

/* time conversions */
uint64_t timespec64_to_ms(const struct timespec64 *time);
struct timespec64 timespec64_from_ns(s64 nsec);
clock_t timespec64_to_clock_t(const struct timespec64 *time);
clock_t clock_ms_to_tick(int ms);
int clock_time_to_tick(const struct timespec *time, bool is_abs_timeout);
int clock_time_abs_to_timespec(int id, struct timespec *result, const struct timespec *abstime);
struct timespec clock_timespec_subtract(const struct timespec *time1, const struct timespec *time2);
struct timespec64 clock_timespec_subtract64(const struct timespec64 *time1,
                                            const struct timespec64 *time2);
struct timespec clock_timespec_add(const struct timespec *time1, const struct timespec *time2);
struct timespec64 clock_timespec_add64(const struct timespec64 *time1,
                                       const struct timespec64 *time2);
struct timespec64 clock_tm_to_timespec64(const struct tm *tm);

/* timespec readers */
void read_realtime_ts64(struct timespec64 *ts);
int ktime_get_monotonic_timespec(clockid_t which_clock, struct timespec64 *tp);

/* 用户态itimerval与内核态itimerspec64之间的拷入/拷出辅助函数 */
int copy_itimerspec64_to_user_itimerval(struct itimerval __user *dest,
                                        const struct itimerspec64 *src);
int copy_user_itimerval_to_itimerspec64(struct itimerspec64 *dest,
                                        struct itimerval __user *src);

/* timezone helpers */
void g_tz_get(struct timezone *tz_prt);
void ktime_get_boottime_ts64(struct timespec64 *ts);

#endif /* __K_TIME__ */
