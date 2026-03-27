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

#include <signal_internal.h>
#include <time/kitimer.h>
#include <time/ktimer.h>
#include <ttosProcess.h>
#include <uaccess.h>

#define timeval_valid(t) (((t)->tv_sec >= 0) && (((unsigned long)(t)->tv_usec) < USEC_PER_SEC))

static struct ktimer *get_process_itimer(pcb_t pcb)
{
    return signal_shared_get(pcb)->itimer;
}

static ktime_t get_process_itimer_interval(pcb_t pcb)
{
    return signal_shared_get(pcb)->itimer_interval;
}

static void set_process_itimer_interval(pcb_t pcb, ktime_t val)
{
    signal_shared_get(pcb)->itimer_interval = val;
}

pid_t itimer_pid_get(struct ktimer *timer)
{
    return (pid_t)(long)timer->priv;
}

void itimer_pid_set(struct ktimer *timer, pid_t pid)
{
    timer->priv = (void *)(long)pid;
}

enum ktimer_restart itimer_handler(struct ktimer *timer)
{
    pid_t pid = itimer_pid_get(timer);

    kernel_signal_send(pid, TO_PROCESS, SIGALRM, SI_KERNEL, NULL);

    return KTIMER_NORESTART;
}

static struct timespec64 read_itimer_remaining(struct ktimer *timer)
{
    ktime_t rem = ktimer_expires_remaining(timer);

    if (ktimer_is_active(timer))
    {
        if (rem <= 0)
        {
            rem = NSEC_PER_USEC;
        }
    }
    else
    {
        rem = 0;
    }

    return ktime_to_timespec64(rem);
}

static void apply_real_itimer(struct itimerspec64 *value, struct itimerspec64 *ovalue)
{
    ktime_t expires = 0;
    irq_flags_t irq_flag;
    struct ktimer *timer = NULL;
    pcb_t cur_pcb = ttosProcessSelf();

retry:

    timer = get_process_itimer(cur_pcb);
    if (ovalue)
    {
        ovalue->it_value = read_itimer_remaining(timer);
        ovalue->it_interval = ktime_to_timespec64(get_process_itimer_interval(cur_pcb));
    }

    if (ktimer_try_to_cancel(timer) < 0)
    {
        goto retry;
    }

    sighand_lock(cur_pcb, &irq_flag);

    expires = timespec64_to_ktime(value->it_value);
    if (expires != 0)
    {
        set_process_itimer_interval(cur_pcb, timespec64_to_ktime(value->it_interval));
        ktimer_start(timer, expires, KTIMER_MODE_REL);
    }
    else
    {
        set_process_itimer_interval(cur_pcb, 0);
    }

    sighand_unlock(cur_pcb, &irq_flag);
}

int apply_itimer(int which, struct itimerspec64 *value, struct itimerspec64 *ovalue)
{
    if (ITIMER_REAL == which)
    {
        apply_real_itimer(value, ovalue);
        return 0;
    }
    else if (ITIMER_VIRTUAL == which || ITIMER_PROF == which)
    {
        printk("ITIMER_VIRTUAL/ITIMER_PROF not supported!\n");
        return -ENOTSUP;
    }
    else
    {
        return -EINVAL;
    }
}

static int read_real_itimer(struct itimerspec64 *value)
{
    irq_flags_t irq_flag;
    pcb_t cur_pcb = ttosProcessSelf();

    struct ktimer *itimer;
    ktime_t interval;

    sighand_lock(cur_pcb, &irq_flag);

    itimer = get_process_itimer(cur_pcb);
    interval = get_process_itimer_interval(cur_pcb);
    value->it_value = read_itimer_remaining(itimer);
    value->it_interval = ktime_to_timespec64(interval);

    sighand_unlock(cur_pcb, &irq_flag);

    return 0;
}

int read_itimer(int which, struct itimerspec64 *value)
{
    if (ITIMER_REAL == which)
    {
        return read_real_itimer(value);
    }
    else if (ITIMER_VIRTUAL == which || ITIMER_PROF == which)
    {
        printk("ITIMER_VIRTUAL/ITIMER_PROF not supported!\n");
        return -ENOTSUP;
    }
    else
    {
        return (-EINVAL);
    }
}

void itimer_rearm(pcb_t pcb)
{
    struct ktimer *itimer = get_process_itimer(pcb);
    ktime_t interval = get_process_itimer_interval(pcb);

    if (!itimer)
    {
        return;
    }

    if (!ktimer_is_enqueued(itimer) && interval != 0)
    {
        ktimer_forward_now(itimer, interval);
        ktimer_restart(itimer);
    }
}

void itimer_rearm_locked(pcb_t pcb)
{
    irq_flags_t irq_flag;

    sighand_lock(pcb, &irq_flag);
    itimer_rearm(pcb);
    sighand_unlock(pcb, &irq_flag);
}

int copy_itimerspec64_to_user_itimerval(struct itimerval __user *dest,
                                        const struct itimerspec64 *src)
{
    struct itimerval k_itimerval = {0};

    k_itimerval.it_interval.tv_sec = src->it_interval.tv_sec;
    k_itimerval.it_interval.tv_usec = src->it_interval.tv_nsec / NSEC_PER_USEC;
    k_itimerval.it_value.tv_sec = src->it_value.tv_sec;
    k_itimerval.it_value.tv_usec = src->it_value.tv_nsec / NSEC_PER_USEC;

    if (copy_to_user(dest, &k_itimerval, sizeof(*dest)))
    {
        return -EFAULT;
    }

    return 0;
}

int copy_user_itimerval_to_itimerspec64(struct itimerspec64 *dest,
                                        struct itimerval __user *src)
{
    struct itimerval kispec = {0};

    if (copy_from_user(&kispec, src, sizeof(struct itimerval)))
    {
        return -EFAULT;
    }

    if (!timeval_valid(&kispec.it_value) || !timeval_valid(&kispec.it_interval))
    {
        return -EINVAL;
    }

    dest->it_interval.tv_sec = kispec.it_interval.tv_sec;
    dest->it_interval.tv_nsec = kispec.it_interval.tv_usec * NSEC_PER_USEC;
    dest->it_value.tv_sec = kispec.it_value.tv_sec;
    dest->it_value.tv_nsec = kispec.it_value.tv_usec * NSEC_PER_USEC;

    return 0;
}
