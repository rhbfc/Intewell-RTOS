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
#include <time.h>
#include <time/ktime.h>
#include <time/posix_timer.h>
#include <ttos.h>
#include <ttosInterTask.inl>
#include <ttosProcess.h>
#include <ttos_time.h>
#include <time/ktime.h>
#include <klog.h>

#define PROCESS_CLOCK make_process_cpuclock(0, CPUCLOCK_SCHED)
#define THREAD_CLOCK make_thread_cpuclock(0, CPUCLOCK_SCHED)

static const struct posix_clock_ops * const builtin_posix_clock_ops[];
static const struct posix_clock_ops realtime_clock_ops, monotonic_clock_ops;
static void read_timer_common(struct posix_timer *timer, struct itimerspec64 *cur_setting);

static int posix_clock_sleep_common(const clockid_t which_clock, int flags,
                                    const struct timespec64 *ts64)
{
   	ktime_t time = timespec64_to_ktime(*ts64);

    enum ktimer_mode mode = (flags & TIMER_ABSTIME) ? KTIMER_MODE_ABS : KTIMER_MODE_REL;

	return ktimer_nanosleep(time, mode, which_clock);
}

void posix_timer_kill_signal(struct posix_timer *posix_timer)
{
    int type;
    pid_t pid;
    TASK_ID task;
    pcb_t target_pcb;

    if (posix_timer->signal_to_thread)
    {
        /*
         * 线程定向 timer 不再长期保存目标线程的 pcb 指针。
         * 这里只记录创建时解析出的 tid，发送时再按 tid 查找，避免线程退出后留下悬空 pcb。
         * 同时补做一次“仍属于同一线程组”的校验，避免 tid 复用后误发到无关线程。
         */
        task = task_get_by_tid(posix_timer->notify_tid);
        target_pcb = task ? task->ppcb : NULL;
        if (!target_pcb || target_pcb->group_leader != posix_timer->pcb->group_leader)
        {
            /*
             * 目标线程已经退出，或者 tid 已经不再属于原线程组。
             * 这次到期通知直接丢弃，同时把 timer 回调提前持有的那份引用归还。
             */
            posix_timer->state = POSIX_TIMER_STOPPED;
            posix_timer_signal_drop(posix_timer);
            return;
        }

        type = TO_THREAD;
        pid  = posix_timer->notify_tid;
    }
    else
    {
        type = TO_PROCESS;
        /* 进程级通知始终以 timer 所属线程组 leader 作为目标。 */
        pid = get_process_pid(posix_timer->pcb);
    }

    posix_timer->posix_timer_sigqueue_seq = posix_timer->posix_timer_signal_seq;
    posix_timer->state = posix_timer->posix_timer_interval ? POSIX_TIMER_SIGNAL_PENDING
                                                           : POSIX_TIMER_STOPPED;

    kernel_signal_send(pid, type, posix_timer->sig_info_ptr->ksiginfo.si_signo, SI_CODE_POSIX_TIMER, &posix_timer->sig_info_ptr->ksiginfo);
}

static enum ktimer_restart posix_timer_handler(struct ktimer *timer)
{
    irq_flags_t flags;
	struct posix_timer *posix_timer = container_of(timer, struct posix_timer, timer);

    spin_lock_irqsave(&posix_timer->posix_timer_signal->siglock, flags);

    if (posix_timer_is_deleted(posix_timer))
    {
        spin_unlock_irqrestore(&posix_timer->posix_timer_signal->siglock, flags);
        return KTIMER_NORESTART;
    }
    else
    {
         posix_timer_getref(posix_timer);
         spin_unlock_irqrestore(&posix_timer->posix_timer_signal->siglock, flags);
    }

	posix_timer_kill_signal(posix_timer);

    /* posix_timer和itimer一样，都在信号获取路径上才去做重启 */
	return KTIMER_NORESTART;
}

static int create_posix_timer_common(struct posix_timer *posix_timer)
{
	ktimer_init(&posix_timer->timer, posix_timer_handler, posix_timer->posix_timer_clock, 0);
    return 0;
}

int do_timer_gettime(timer_t timer_id,  struct itimerspec64 *setting)
{
	memset(setting, 0, sizeof(*setting));

	struct posix_timer *posix_timer = posix_timer_get((int)(long)timer_id);
	if (!posix_timer)
	{
		return -EINVAL;
	}

    if (posix_timer->pcb->group_leader != ttosProcessSelf()->group_leader)
    {
        posix_timer_putref(posix_timer);
        return -EINVAL;
    }

    posix_timer->clock_ops->read_timer(posix_timer, setting);
    posix_timer_putref(posix_timer);

	return 0;
}

int do_timer_settime(timer_t timer_id, int tmr_flags, struct itimerspec64 *new_spec64,
			    struct itimerspec64 *old_spec64)
{
    int ret;

	if (!timespec64_valid(&new_spec64->it_interval) ||
	    !timespec64_valid(&new_spec64->it_value))
    {
      	return -EINVAL;
    }

	if (old_spec64)
    {
		memset(old_spec64, 0, sizeof(*old_spec64));
    }

	struct posix_timer *posix_timer = posix_timer_get((int)(long)timer_id);
	if (!posix_timer)
	{
        /* timer已经无效 */
		return -EINVAL;
	}

    if (posix_timer->pcb->group_leader != ttosProcessSelf()->group_leader)
    {
        posix_timer_putref(posix_timer);
        return -EINVAL;
    }

	if (old_spec64)
    {
		old_spec64->it_interval = ktime_to_timespec64(posix_timer->posix_timer_interval);
    }

    /* 阻止信号投递和timer重启 */
	posix_timer->posix_timer_signal_seq++;

	do
    {
		ret = posix_timer->clock_ops->configure_timer(posix_timer, tmr_flags, new_spec64, old_spec64);
    } while (ret == TIMER_RETRY);

    posix_timer_putref(posix_timer);
	return ret;
}

void posix_timer_set_common(struct posix_timer *timer, struct itimerspec64 *new_setting)
{
	if (new_setting->it_value.tv_sec || new_setting->it_value.tv_nsec)
    {
        timer->posix_timer_interval = timespec64_to_ktime(new_setting->it_interval);
    }
	else
    {
		timer->posix_timer_interval = 0;
    }

	timer->posix_timer_overrun_last = 0;
	timer->posix_timer_overrun = -1LL;
}

static ktime_t read_monotonic_ktime(clockid_t which_clock)
{
	return ktime_now();
}

static int configure_posix_timer_common(struct posix_timer *posix_timer, int flags,
                                        struct itimerspec64 *new_setting,
                                        struct itimerspec64 *old_setting)
{
	const struct posix_clock_ops *clock_ops = posix_timer->clock_ops;
	bool sigev_none;
	ktime_t expires;

	if (old_setting)
    {
		read_timer_common(posix_timer, old_setting);
    }

	if (clock_ops->try_cancel_timer(posix_timer) < 0)
    {
		return TIMER_RETRY;
    }

	posix_timer->state = POSIX_TIMER_STOPPED;

	posix_timer_set_common(posix_timer, new_setting);

	if (!new_setting->it_value.tv_sec && !new_setting->it_value.tv_nsec)
    {
		return 0;
    }

	expires = timespec64_to_ktime(new_setting->it_value);

    /* SIGEV_NONE 表示定时器到期后，不发送信号、不创建线程、不做任何异步通知(当前实现会创建timer，但不会启动) */
	sigev_none = posix_timer->posix_timer_sigev_notify == SIGEV_NONE;

	clock_ops->arm_timer(posix_timer, expires, flags & TIMER_ABSTIME, sigev_none);
	if (!sigev_none)
    {
		posix_timer->state = POSIX_TIMER_RUNNING;
    }

	return 0;
}

static void read_timer_common(struct posix_timer *timer, struct itimerspec64 *cur_setting)
{
	const struct posix_clock_ops *clock_ops = timer->clock_ops;
	ktime_t now, remaining, it_interval;
	bool sig_none;

	sig_none = timer->posix_timer_sigev_notify == SIGEV_NONE;
	it_interval = timer->posix_timer_interval;

	if (it_interval)
    {
		cur_setting->it_interval = ktime_to_timespec64(it_interval);
	}
    else if (timer->state == POSIX_TIMER_STOPPED)
    {
		if (!sig_none)
        {
			return;
        }
	}

	now = clock_ops->read_ktime(timer->posix_timer_clock);

	if (it_interval && timer->state != POSIX_TIMER_RUNNING)
    {
		timer->posix_timer_overrun += clock_ops->advance_timer(timer, now);
    }

	remaining = clock_ops->remaining_time(timer, now);

	if (remaining <= 0)
    {
		if (!sig_none)
			cur_setting->it_value.tv_nsec = 1;
	}
    else
    {
		cur_setting->it_value = ktime_to_timespec64(remaining);
	}
}

static int remove_posix_timer_common(struct posix_timer *timer)
{
	const struct posix_clock_ops *clock_ops = timer->clock_ops;

	if (clock_ops->try_cancel_timer(timer) < 0)
    {
		return TIMER_RETRY;
    }

	timer->state = POSIX_TIMER_STOPPED;

	return 0;
}

static int try_cancel_posix_timer_common(struct posix_timer *posix_timer)
{
	return ktimer_try_to_cancel(&posix_timer->timer);
}

static void task_enum(pcb_t pcb, void *param)
{
    struct timespec64 *tp = param;

    tp->tv_sec += pcb->utime.tv_sec;
    tp->tv_nsec += pcb->utime.tv_nsec;

    while (tp->tv_nsec > NSEC_PER_SEC)
    {
        tp->tv_sec++;
        tp->tv_nsec -= NSEC_PER_SEC;
    }
}

static int posix_get_process_timespec(const clockid_t which_clock, struct timespec64 *tp)
{
    int pid = CPUCLOCK_PID(which_clock);
    pcb_t pcb;

    if (pid == 0)
    {
        pcb = ttosProcessSelf();
    }
    else
    {
        TASK_ID task = task_get_by_tid(pid);
        if (task == NULL)
        {
            return -EINVAL;
        }
        pcb = task->ppcb;
    }

    if (pcb == NULL)
    {
        return -EINVAL;
    }

    tp->tv_sec = 0;
    tp->tv_nsec = 0;
    foreach_task_group(pcb, task_enum, tp);
    return 0;
}

static int process_get_process_timespec(const clockid_t which_clock, struct timespec64 *tp)
{
    return posix_get_process_timespec(PROCESS_CLOCK, tp);
}

static int process_get_thread_timespec(const clockid_t which_clock, struct timespec64 *tp)
{
    return posix_get_process_timespec(THREAD_CLOCK, tp);
}

static void arm_posix_timer_common(struct posix_timer *posix_timer, ktime_t expires,
			           bool is_absolute, bool sigev_none)
{
	struct ktimer *timer = &posix_timer->timer;
	enum   ktimer_mode mode;

	mode = is_absolute ? KTIMER_MODE_ABS : KTIMER_MODE_REL;

	if (posix_timer->posix_timer_clock == CLOCK_REALTIME)
    {
        /* 使用相对时间的CLOCK_REALTIME 不受时间修改影响*/
		posix_timer->clock_ops = is_absolute ? &realtime_clock_ops : &monotonic_clock_ops;
    }

	ktimer_init(&posix_timer->timer, posix_timer_handler, posix_timer->posix_timer_clock, mode);

	if (!is_absolute)
    {
		expires = ktime_add_safe(expires, ktimer_get_time(timer));
    }

	ktimer_set_expires(timer, expires);

	if (!sigev_none)
    {
        ktimer_start_expires(timer, KTIMER_MODE_ABS);
    }
}

static s64 advance_posix_timer_common(struct posix_timer *posix_timer, ktime_t now)
{
	struct ktimer *timer = &posix_timer->timer;

	return ktimer_forward(timer, now, posix_timer->posix_timer_interval);
}

static void rearm_posix_timer_common(struct posix_timer *posix_timer)
{
	struct ktimer *timer = &posix_timer->timer;

	posix_timer->posix_timer_overrun += ktimer_forward_now(timer, posix_timer->posix_timer_interval);
	ktimer_restart(timer);
}

static ktime_t read_posix_timer_remaining_common(struct posix_timer *posix_timer, ktime_t now)
{
	struct ktimer *timer = &posix_timer->timer;

	ktime_t rem = ktime_sub(timer->queue_entry.expires, now);

	return rem;
}

static int ktimer_get_res(clockid_t which_clock, struct timespec64 *tp)
{
	u64 nsec = ktimer_get_resolution_ns();

	tp->tv_sec  = nsec / NSEC_PER_SEC;
	tp->tv_nsec = nsec % NSEC_PER_SEC;

	return 0;
}

static int update_realtime_clock(const clockid_t which_clock,
				 const struct timespec64 *tp)
{
	return kernel_settimeofday64(tp, NULL);
}

int timer_overrun_to_int(struct posix_timer *posix_timer)
{
	if (posix_timer->posix_timer_overrun_last > (s64)INT_MAX)
    {
		return INT_MAX;
    }

	return (int)posix_timer->posix_timer_overrun_last;
}

static int read_realtime_timespec(const clockid_t which_clock, struct timespec64 *tp)
{
    read_realtime_ts64(tp);
    return 0;
}

static ktime_t read_realtime_ktime(const clockid_t which_clock)
{
    return ktime_get_real();
}

static const struct posix_clock_ops realtime_clock_ops =
{
    .read_res              = ktimer_get_res,
    .read_timespec         = read_realtime_timespec,
    .read_ktime            = read_realtime_ktime,
    .update_time           = update_realtime_clock,
    .sleep                 = posix_clock_sleep_common,
    .create_timer          = create_posix_timer_common,
    .configure_timer       = configure_posix_timer_common,
    .read_timer            = read_timer_common,
    .remove_timer          = remove_posix_timer_common,
    .rearm_timer           = rearm_posix_timer_common,
    .advance_timer         = advance_posix_timer_common,
    .remaining_time        = read_posix_timer_remaining_common,
    .try_cancel_timer      = try_cancel_posix_timer_common,
    .arm_timer             = arm_posix_timer_common,
};

static const struct posix_clock_ops monotonic_clock_ops =
{
    .read_res              = ktimer_get_res,
    .read_timespec         = ktime_get_monotonic_timespec,
    .read_ktime            = read_monotonic_ktime,
    .update_time           = NULL,
    .sleep                 = posix_clock_sleep_common,
    .create_timer          = create_posix_timer_common,
    .configure_timer       = configure_posix_timer_common,
    .read_timer            = read_timer_common,
    .remove_timer          = remove_posix_timer_common,
    .rearm_timer           = rearm_posix_timer_common,
    .advance_timer         = advance_posix_timer_common,
    .remaining_time        = read_posix_timer_remaining_common,
    .try_cancel_timer      = try_cancel_posix_timer_common,
    .arm_timer             = arm_posix_timer_common,
};

static const struct posix_clock_ops process_cpu_clock_ops =
{
    .read_res              = ktimer_get_res,
    .read_timespec         = process_get_process_timespec,
};

static const struct posix_clock_ops thread_cpu_clock_ops =
{
    .read_res              = ktimer_get_res,
    .read_timespec         = process_get_thread_timespec,
};

static const struct posix_clock_ops fallback_cpu_clock_ops =
{
    .read_res              = ktimer_get_res,
    .read_timespec         = posix_get_process_timespec,
};

static const struct posix_clock_ops *const builtin_posix_clock_ops[] =
{
    [CLOCK_REALTIME]           = &realtime_clock_ops,
    [CLOCK_MONOTONIC]          = &monotonic_clock_ops,
    [CLOCK_PROCESS_CPUTIME_ID] = &process_cpu_clock_ops,
    [CLOCK_THREAD_CPUTIME_ID]  = &thread_cpu_clock_ops,
    [CLOCK_MONOTONIC_RAW]      = &monotonic_clock_ops,
    [CLOCK_REALTIME_COARSE]    = &realtime_clock_ops,
    [CLOCK_MONOTONIC_COARSE]   = &monotonic_clock_ops,
    [CLOCK_BOOTTIME]           = &monotonic_clock_ops,
    [CLOCK_REALTIME_ALARM]     = &realtime_clock_ops,
    [CLOCK_BOOTTIME_ALARM]     = &monotonic_clock_ops,
    [CLOCK_TAI]                = NULL,
};

const struct posix_clock_ops *clock_id_to_ops(const clockid_t id)
{
    if (id < 0)
    {
        return (id & CLOCKFD_MASK) == CLOCKFD ? NULL : &fallback_cpu_clock_ops;
    }

    if (id >= (sizeof(builtin_posix_clock_ops) / sizeof(builtin_posix_clock_ops[0])))
        return NULL;

    return builtin_posix_clock_ops[id];
}
