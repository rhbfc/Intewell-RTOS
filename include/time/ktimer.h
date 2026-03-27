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

#ifndef __KTIMER_H
#define __KTIMER_H
#include <time/ktime.h>
#include <time/ktimerqueue.h>

#define KTIMER_INACTIVE	0x1
#define KTIMER_ENQUEUED	0x2

enum ktimer_mode
{
	KTIMER_MODE_ABS	            = 0x00,
	KTIMER_MODE_REL	            = 0x01,
	KTIMER_MODE_HARD	        = 0x08,
};

enum ktimer_base_type
{
	KTIMER_BASE_MONOTONIC,
	KTIMER_BASE_REALTIME,
	KTIMER_BASE_BOOTTIME,
	KTIMER_BASE_TAI,
	KTIMER_MAX_CLOCK_BASES,
};

typedef struct T_TTOS_TaskControlBlock_Struct T_TTOS_TaskControlBlock;

enum ktimer_restart
{
	KTIMER_NORESTART,
	KTIMER_RESTART
};

struct ktimer_clock_base
{
	struct ktimer_cpu_base	*cpu_base;
	uint32_t		         base_type;
	clockid_t		         clock_id;
	struct ktimer		    *running_timer;
	struct ktimerqueue	     queue;
	ktime_t			       (*read_now)(void);
	ktime_t			         time_offset;
};

struct ktimer_cpu_base
{
    uint32_t					cpu;
    uint32_t					active_mask;
    uint32_t					in_irq;
	ktime_t						next_deadline;
	struct ktimer			   *next_expiring;
	struct ktimer_clock_base	bases[KTIMER_MAX_CLOCK_BASES];
};

struct ktimer
{
	struct ktimerqueue_entry	queue_entry;
	struct ktimer_clock_base   *base;
	atomic_t				    state;
	void                       *priv;
	clockid_t                   clock_id;
	enum ktimer_restart        (*function)(struct ktimer *);
};

struct ktimer_sleep_ctx
{
	struct ktimer            timer;
	T_TTOS_TaskControlBlock  *task;
};

struct restart_info;

/* ktime helpers used by ktimer */
ktime_t ktime_add_safe(const ktime_t lhs, const ktime_t rhs);
ktime_t ktime_get_real(void);

/* ktimer core control */
int32_t ktimer_init(struct ktimer *timer, enum ktimer_restart (*function)(struct ktimer *),
		   clockid_t clock_id, enum ktimer_mode mode);
void ktimer_start(struct ktimer *timer, ktime_t time, const enum ktimer_mode mode);
void ktimer_restart(struct ktimer *timer);
int32_t ktimer_try_to_cancel(struct ktimer *timer);
int32_t ktimer_cancel(struct ktimer *timer);

/* ktimer state and time queries */
bool ktimer_is_enqueued(struct ktimer *timer);
bool ktimer_is_inactive(struct ktimer *timer);
bool ktimer_is_active(struct ktimer *timer);
ktime_t ktimer_get_expires(const struct ktimer *timer);
ktime_t ktimer_expires_remaining(const struct ktimer *timer);
ktime_t ktimer_remaining(const struct ktimer *timer);
ktime_t ktimer_get_time(struct ktimer *timer);
uint64_t ktimer_get_resolution_ns(void);

/* ktimer expiration updates */
void ktimer_set_expires(struct ktimer *timer, ktime_t time);
void ktimer_add_expires(struct ktimer *timer, ktime_t time);
void ktimer_start_expires(struct ktimer *timer, enum ktimer_mode mode);
uint64_t ktimer_forward(struct ktimer *timer, ktime_t now, ktime_t interval);
uint64_t ktimer_forward_now(struct ktimer *timer, ktime_t interval);

/* nanosleep helpers */
void ktimer_sleep_ctx_start_expires(struct ktimer_sleep_ctx *sleep_ctx, enum ktimer_mode mode);
long ktimer_nanosleep(ktime_t rqtp, const enum ktimer_mode mode, const clockid_t clock_id);
int32_t write_nanosleep_remain(struct restart_info *restart, struct timespec64 *ts);

/* interval timer helpers */
int32_t read_itimer(int32_t which, struct itimerspec64 *value);
int32_t apply_itimer(int32_t which, struct itimerspec64 *value, struct itimerspec64 *ovalue);

/* system integration */
void ktimer_handle_clock_change(void);
void ktimer_interrupt(uint32_t irq, void *arg);
void ktimer_sys_init(void);
int32_t ktimer_set_resolution(uint64_t freq);

/* cross-subsystem cleanup */
void posix_timer_clean(void);

#endif /*__KTIMER_H */
