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

#ifndef __POSXI_TIMER__
#define __POSXI_TIMER__

#include <bits/alltypes.h>
#include "ktime.h"
#include <spinlock.h>
#include <time/kitimer.h>
#include <limits.h>

#define TIMER_RETRY 1
#ifndef TIMER_ANY_ID
#define TIMER_ANY_ID INT_MIN
#endif

typedef struct sigevent sigevent_t;
typedef struct ksiginfo_entry ksiginfo_entry_t;

#define CPUCLOCK_PID(clock)     ((pid_t) ~((clock) >> 3))
#define CPUCLOCK_PERTHREAD(clock) \
    (((clock) & (clockid_t) CPUCLOCK_PERTHREAD_MASK) != 0)

#define CPUCLOCK_PERTHREAD_MASK 4
#define CPUCLOCK_WHICH(clock)   ((clock) & (clockid_t) CPUCLOCK_CLOCK_MASK)
#define CPUCLOCK_CLOCK_MASK     3
#define CPUCLOCK_PROF           0
#define CPUCLOCK_VIRT           1
#define CPUCLOCK_SCHED          2
#define CPUCLOCK_MAX            3
#define CLOCKFD                 CPUCLOCK_MAX
#define CLOCKFD_MASK            (CPUCLOCK_PERTHREAD_MASK|CPUCLOCK_CLOCK_MASK)

struct __kernel_timex;
struct shared_signal;

static inline clockid_t make_process_cpuclock(const unsigned int pid,
		const clockid_t clock)
{
	return ((~pid) << 3) | clock;
}

static inline clockid_t make_thread_cpuclock(const unsigned int tid,
		const clockid_t clock)
{
	return make_process_cpuclock(tid, clock | CPUCLOCK_PERTHREAD_MASK);
}

struct posix_clock_ops {
    int (*read_res)(const clockid_t which_clock, struct timespec64 *ts64);
    int (*update_time)(const clockid_t which_clock, const struct timespec64 *ts64);
    int (*read_timespec)(const clockid_t which_clock, struct timespec64 *ts64);
    ktime_t (*read_ktime)(const clockid_t which_clock);
    int (*create_timer)(struct posix_timer *timer);
    int (*sleep)(const clockid_t which_clock, int flags, const struct timespec64 *ts64);
    int (*configure_timer)(struct posix_timer *posix_timer, int flags,
                           struct itimerspec64 *new_setting,
                           struct itimerspec64 *old_setting);
    int (*remove_timer)(struct posix_timer *posix_timer);
    void (*read_timer)(struct posix_timer *posix_timer, struct itimerspec64 *cur_setting);
    void (*rearm_timer)(struct posix_timer *posix_timer);
    s64 (*advance_timer)(struct posix_timer *posix_timer, ktime_t now);
    ktime_t (*remaining_time)(struct posix_timer *posix_timer, ktime_t now);
    int (*try_cancel_timer)(struct posix_timer *posix_timer);
    void (*arm_timer)(struct posix_timer *posix_timer, ktime_t expires,
                      bool absolute, bool sigev_none);
};

const struct posix_clock_ops *clock_id_to_ops(const clockid_t id);

/* POSIX timer lifecycle */
int posix_timer_create(clockid_t which_clock, sigevent_t *event, timer_t __user *created_timer_id);
struct posix_timer *posix_timer_detach(int timer_id);
struct posix_timer *posix_timer_get(int index);
void posix_timer_delete(struct posix_timer *timer);
void posix_timer_clean(void);
void posix_timer_clean_shared(struct shared_signal *signal);

/* POSIX timer reference and state helpers */
void posix_timer_free(struct posix_timer *timer);
void posix_timer_getref(struct posix_timer *timer);
int posix_timer_putref(struct posix_timer *timer);
bool posix_timer_is_deleted(struct posix_timer *timer);

/* POSIX timer signal helpers */
bool posixtimer_signal_process(ksiginfo_entry_t *siginfo);
void posix_timer_signal_drop(void *priv);
void posix_timer_signal_lock(struct posix_timer *timer, irq_flags_t *irq_flag);
void posix_timer_signal_unlock(struct posix_timer *timer, irq_flags_t *irq_flag);

/* User-space time structure import/export */
int copy_timespec64_to_user(const struct timespec64 *src, struct timespec64 __user *dest);
int copy_itimerspec64_to_user(const struct itimerspec64 *src, struct itimerspec64 __user *dest);
int copy_timespec64_from_user(struct timespec64 *dest, const struct timespec64 __user *src);
int copy_itimerspec64_from_user(struct itimerspec64 *dest, const struct itimerspec64 __user *src);

/* POSIX timer syscall backends */
int do_timer_settime(timer_t timer_id, int tmr_flags, struct itimerspec64 *new_spec64,
		     struct itimerspec64 *old_spec64);
int do_timer_gettime(timer_t timer_id, struct itimerspec64 *setting);
#endif   /* __POSXI_TIMER__ */
