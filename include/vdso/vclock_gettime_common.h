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

#ifndef __VDSO_VCLOCK_GETTIME_COMMON_H__
#define __VDSO_VCLOCK_GETTIME_COMMON_H__

#include <vdso_datapage.h>

typedef int clockid_t;
typedef long __kernel_old_time_t;

struct __kernel_timespec
{
    long long tv_sec;
    long tv_nsec;
};

struct __kernel_timespec64
{
    long long tv_sec;
    long long tv_nsec;
};

struct __kernel_old_timeval
{
    long tv_sec;
    long tv_usec;
};

struct timezone
{
    int tz_minuteswest;
    int tz_dsttime;
};

#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1
#define CLOCK_MONOTONIC_RAW 4
#define CLOCK_REALTIME_COARSE 5
#define CLOCK_MONOTONIC_COARSE 6
#define CLOCK_BOOTTIME 7

#define EINVAL 22
#define NSEC_PER_SEC 1000000000ULL

static inline unsigned long long vdso_arch_read_cycles(void);
static inline const struct ttos_vdso_data *vdso_arch_data_ptr(void);

struct vdso_snapshot
{
    unsigned long long cycle_freq;
    long long realtime_offset_ns;
    unsigned long long clock_res_ns;
};

static inline void vdso_barrier(void)
{
    __asm__ __volatile__("" ::: "memory");
}

static inline unsigned long long vdso_cycles_to_ns(unsigned long long cycles, unsigned long long freq)
{
    unsigned long long sec = cycles / freq;
    unsigned long long rem = cycles % freq;

    return sec * NSEC_PER_SEC + (rem * NSEC_PER_SEC) / freq;
}

static inline int vdso_clock_supported(clockid_t clock)
{
    switch (clock)
    {
        case CLOCK_REALTIME:
        case CLOCK_REALTIME_COARSE:
        case CLOCK_MONOTONIC:
        case CLOCK_MONOTONIC_RAW:
        case CLOCK_MONOTONIC_COARSE:
        case CLOCK_BOOTTIME:
            return 1;
        default:
            return 0;
    }
}

static void vdso_read_snapshot(struct vdso_snapshot *snap)
{
    const struct ttos_vdso_data *data = vdso_arch_data_ptr();

    vdso_barrier();
    snap->cycle_freq         = data->cycle_freq;
    snap->realtime_offset_ns = data->realtime_offset_ns;
    snap->clock_res_ns       = data->clock_res_ns;
    vdso_barrier();
}

static int vdso_read_ns(clockid_t clock, long long *ns_out)
{
    struct vdso_snapshot snap;
    long long ns;
    int ret;

    vdso_read_snapshot(&snap);

    ns = (long long)vdso_cycles_to_ns(vdso_arch_read_cycles(), (unsigned long long)snap.cycle_freq);

    switch (clock)
    {
        case CLOCK_REALTIME:
        case CLOCK_REALTIME_COARSE:
            ns += snap.realtime_offset_ns;
            break;
        case CLOCK_MONOTONIC:
        case CLOCK_MONOTONIC_RAW:
        case CLOCK_MONOTONIC_COARSE:
        case CLOCK_BOOTTIME:
            break;
        default:
            return -EINVAL;
    }

    if (ns < 0)
    {
        ns = 0;
    }

    *ns_out = ns;
    return 0;
}

int __vdso_clock_gettime(clockid_t clock, struct __kernel_timespec *ts)
{
    long long ns;
    unsigned long long sec;
    unsigned long long ns_u64;
    int ret;

    if (!ts)
    {
        return -EINVAL;
    }

    ret = vdso_read_ns(clock, &ns);
    if (ret < 0)
    {
        return ret;
    }

    ns_u64 = (unsigned long long)ns;
 
    ts->tv_sec  = ns_u64 / NSEC_PER_SEC;
    ts->tv_nsec = ns_u64 % NSEC_PER_SEC;

    return 0;
}

int __vdso_clock_gettime64(clockid_t clock, struct __kernel_timespec64 *ts)
{
    long long ns;
    unsigned long long sec;
    unsigned long long ns_u64;
    int ret;

    if (!ts)
    {
        return -EINVAL;
    }

    ret = vdso_read_ns(clock, &ns);
    if (ret < 0)
    {
        return ret;
    }

    ns_u64 = (unsigned long long)ns;

    ts->tv_sec  = ns_u64 / NSEC_PER_SEC;
    ts->tv_nsec = ns_u64 % NSEC_PER_SEC;

    return 0;
}

#endif /* __VDSO_VCLOCK_GETTIME_COMMON_H__ */
