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

#ifndef __KITIMER_H
#define __KITIMER_H

#include <list.h>
#include <time/ktime.h>
#include <time/ktimer.h>
#include <ttosRBTree.h>
#include <pid.h>

struct itimerspec64
{
    struct timespec64 it_interval;
    struct timespec64 it_value;
};

struct itimerspec32
{
    struct timespec32 it_interval;
    struct timespec32 it_value;
};

enum posix_timer_state
{
    POSIX_TIMER_STOPPED,        /* timer 当前没有挂载下一次到期 */
    POSIX_TIMER_RUNNING,        /* timer 的下一次到期已经挂到内核 timer */
    POSIX_TIMER_SIGNAL_PENDING, /* timer 已到期并发出信号，等待投递后决定是否续期 */
};

struct posix_timer
{
    /* timer 所属的线程组 leader；用于权限校验和 shared_signal 生命周期绑定 */
    pcb_t                  pcb;
    /* SIGEV_THREAD_ID 目标线程的 tid；只在 signal_to_thread=true 时使用 */
    pid_t                  notify_tid;
    struct list_head       node;
    clockid_t              posix_timer_clock;
    const struct posix_clock_ops *clock_ops;
    struct ksiginfo_entry *sig_info_ptr;
    struct shared_signal	   *posix_timer_signal;

    struct rb_node  posix_timer_rb_node;

    int32_t         posix_timer_id;
    enum posix_timer_state state;

    int64_t			posix_timer_overrun;
	int64_t			posix_timer_overrun_last;

    int32_t			posix_timer_sigev_notify;
    ktime_t		    posix_timer_interval;

    uint32_t		posix_timer_signal_seq;
	uint32_t		posix_timer_sigqueue_seq;
    atomic_t        refcnt;
    atomic_t        posix_timer_deleted;

    bool            signal_to_thread;

    struct ktimer	timer;
};

struct ktimer;

pid_t itimer_pid_get (struct ktimer *timer);
void itimer_pid_set  (struct ktimer *timer, pid_t pid);

#endif
