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

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <ttos.h>
#include <ttosBase.h>
#include <ttosProcess.h>
#include <signal_internal.h>
#include <tglock.h>
#include <uaccess.h>
#include <trace.h>
#include <time/ktime.h>
#include <time/kitimer.h>
#include <time/posix_timer.h>

#undef KLOG_TAG
#define KLOG_TAG "signal"
#include <klog.h>

#undef KLOG_D
#define KLOG_D(...)

void itimer_rearm(pcb_t pcb);
void itimer_rearm_locked(pcb_t pcb);

/* ==================== 信号等待 ==================== */

int clockTimespecToTicks(const struct timespec *time)
{
    int tick = 0;
    int tick_ns = 0;
    unsigned int sys_clk_rate = TTOS_GetSysClkRate();
    tick_ns = time->tv_nsec / (NSEC_PER_SEC / sys_clk_rate);
    tick = time->tv_sec * sys_clk_rate + tick_ns;
    if (tick_ns * (NSEC_PER_SEC / sys_clk_rate) < time->tv_nsec)
    {
        tick++;
    }
    if (tick < 0)
    {
        tick = 0;
    }
    return tick;
}

/* 等待sigset中的信号，等到后，丢弃该信号，返回 */
int kernel_sigtimedwait(const k_sigset_t *sigset, siginfo_t *info, const struct timespec64 *kts64,
                        size_t sigsize)
{
	pcb_t pcb;
	k_sigset_t mask = *sigset;
	int sig = 0;
    enum ktimer_mode mode = KTIMER_MODE_ABS;
    ktime_t timeout_ns = KTIME_MAX;
    T_TTOS_TaskControlBlock *task = ttosGetRunningTask();
    siginfo_t kinfo = {0};
    siginfo_t *kinfo_ptr = info ? &kinfo : NULL;

    if (task->ppcb)
    {
        pcb = (pcb_t)task->ppcb;

        /* 不需要剩余时间 */
        pcb->restart.nanosleep.rmtp_exist = RMTP_NONE;
    }

	if (kts64)
    {
		if (!timespec64_valid(kts64))
        {
			return -EINVAL;
        }

		timeout_ns = timespec64_to_ns(kts64);

        mode = KTIMER_MODE_REL;
	}

	signal_set_del_immutable(&mask);

    /* 计算出需要阻塞的信号set */
	signal_set_not(&mask, &mask);

    sig = signal_dequeue(pcb, &mask, kinfo_ptr);
	if (!sig && timeout_ns)
    {
		pcb->real_blocked = pcb->blocked;
		signal_set_and(&pcb->blocked, &pcb->blocked, &mask);

		signal_pending_recalc_current();

        ktimer_nanosleep(timeout_ns, mode, CLOCK_MONOTONIC);

		sig = signal_dequeue(pcb, &mask, kinfo_ptr);
        signal_mask_apply_current(&pcb->real_blocked);
		signal_set_clear(&pcb->real_blocked);
        signal_pending_recalc_current();
	}

    if (sig > 0 && info)
    {
        if (copy_to_user(info, &kinfo, sizeof(kinfo)))
        {
            return -EFAULT;
        }
    }

    if (sig)
    {
        return sig;
    }

    if (!timeout_ns || task->wait.returnCode == TTOS_TIMEOUT)
    {
        return -EAGAIN;
    }
    else if (task->wait.returnCode == TTOS_SIGNAL_INTR)
    {
        return -EINTR;
    }

    assert(0);
}

/* 
 * 该函数临时替换任务的信号屏蔽码,然后去等待该屏蔽码外的所有信号,等到某一信号后，最后再恢复原始的信号屏蔽集
 * 等到的信号按照信号处理流程处理,不会丢弃信号(rt_sigtimedwait会丢弃信号)
 */
void kernel_signal_suspend(const k_sigset_t *set)
{
    pcb_t pcb = ttosProcessSelf();
    pcb->saved_sigmask = pcb->blocked;

    signal_mask_apply_current(set);

    while (!pcb_signal_pending(pcb))
    {
        ktimer_nanosleep(INT64_MAX, KTIMER_MODE_ABS, CLOCK_MONOTONIC);
    }

    pcb->need_restore_blocked = true;
}

/* ==================== stop/exit 处理 ==================== */

void signal_notify_parent_and_leader(pcb_t child_process, int signo, bool is_stop)
{
    pcb_t leader = child_process->group_leader;
    int si_code;
    pcb_t parent_process = leader->parent;

    if (!parent_process)
    {
        return;
    }

    /*
     * 这里统一产出 wait4() 可见的状态：
     *   - stop:      PROCESS_CREATE_STAT_STOPPED(signo)
     *   - continue:  PROCESS_CREATE_STAT_CONTINUED
     */
    if (is_stop)
    {
        si_code = CLD_STOPPED;
        leader->state = PROCESS_CREATE_STAT_STOPPED(signo);
        leader->exit_signal = signo;
    }
    else
    {
        si_code = CLD_CONTINUED;
        leader->state = PROCESS_CREATE_STAT_CONTINUED;
    }

    /* 唤醒waitpid上的任务 */
    process_wakeup_waiter(leader);

    /*
        检测是否需要通知父进程状态改变,根据SIGCHLD 对应handler 的 SA_NOCLDSTOP标志。
        如果设置了SA_NOCLDSTOP，则表示子进程stop或者stop后continue 不通知父进程状态改变,
        即不发SIGCHLD。否则，会发SIGCHLD通知父进程状态改变。
    */
    if (signal_should_report_status(parent_process, signo))
    {
        ksiginfo_t info = {0};
        info.si_pid = get_process_pid(leader);
        info.si_uid = 0;
        info.si_status = signo;
        pid_t pid = get_process_pid(parent_process);

        /* 给父进程发SIGCHLD信号,通知状态变化 */
        kernel_signal_send(pid, TO_PROCESS, SIGCHLD, si_code, &info);
    }
}

static void stop_process(pcb_t pcb)
{
    pcb_t tmp_pcb = NULL;
    irq_flags_t irq_flag = 0;

    tg_lock(pcb->group_leader, &irq_flag);

    /* 收到stop信号，需要停止线程组中的所有线程 */
    for_each_thread_in_tgroup(pcb, tmp_pcb)
    {
        /* 给除了自己以外非TTOS_TASK_STOPPED_BY_SIGNAL状态的任务发SIGSTOP信号 */
        if (tmp_pcb != pcb && !(TTOS_TASK_STOPPED_BY_SIGNAL & tmp_pcb->taskControlId->state))
        {
            tmp_pcb->taskControlId->state |= TTOS_TASK_STOPPED_BY_SIGNAL;
            kernel_signal_send(tmp_pcb->taskControlId->tid, TO_THREAD, SIGSTOP, SI_KERNEL, NULL);
        }
    }

    errno = (0);

    pcb->group_leader->jobctl_stopped = true;
    pcb->taskControlId->state |= TTOS_TASK_STOPPED_BY_SIGNAL;
    tg_unlock(pcb->group_leader, &irq_flag);

    /* 线程真正挂起后，通常只有 SIGKILL / SIGCONT 才会把它重新拉起。 */
    (void)TTOS_SignalSuspendTask(pcb->taskControlId);
}

void do_signal_stop(pcb_t pcb, int signo)
{
    /* 这里负责真正执行已经被接收侧确认过的 group-stop。 */
    signal_notify_parent_and_leader(pcb, signo, true);
    stop_process(pcb);
}

void exit_del(int signo, pcb_t pcb)
{
    int exit_code = pcb->group_leader->group_exit_status.exit_code;

    //如果进程组已经退出
    if (pcb->group_leader->group_exit_status.is_terminated)
    {
        if (pcb->group_leader->group_exit_status.is_normal_exit)
        {
            pcb->exit_code = exit_code;
            //自己退出
            process_exit(pcb);
        }
        else
        {
            pcb->exit_signal = exit_code;
            //自己退出
            process_exit(pcb);
        }
    }
    else //如果进程组未退出，则退出组
    {
        if (pcb->group_exit_status.group_request_terminate)
        {
            process_exit_group(exit_code, pcb->group_leader->group_exit_status.is_normal_exit);
        }
        else
        {
            process_exit_group(signo, false);
        }
    }
}

/* ==================== signal_fetch_deliverable 主循环 ==================== */

int peek_signal(pcb_t pcb, struct k_sigpending_queue * *pending, bool *is_private_signal)
{
    int signo = 0;

    k_sigset_t *sig_mask = NULL;

    /* 检查线程是否有pending的信号需要处理 */
    if (!sigqueue_isempty(sigqueue_private_get(pcb)))
    {
        *pending = sigqueue_private_get(pcb);
        sig_mask = &pcb->blocked;
        signo = sigqueue_peek(*pending, sig_mask);
        *is_private_signal = true;
    }

    /* 检查进程是否有信号需要处理 */
    struct shared_signal *signal = signal_shared_get(pcb);
    if (!signo && !sigqueue_isempty(&signal->sig_queue))
    {
        *pending = &signal->sig_queue;
        sig_mask = &pcb->blocked;
        signo = sigqueue_peek(*pending, sig_mask);
        *is_private_signal = false;
    }

    return signo;
}

static void signal_mark_jobctl_stop(pcb_t pcb, int signo)
{
    pcb_t leader = pcb->group_leader;

    /* 只记录“下一轮 signal_fetch_deliverable() 需要先消费什么控制事件”，不在发送侧直接执行 stop。 */
    leader->jobctl = signal_ctrl_set_stop_signo(leader->jobctl, signo);
    leader->jobctl &= ~SIGNAL_CTRL_CONTINUE_REPORT_PENDING;
    leader->jobctl |= SIGNAL_CTRL_GROUP_STOP_PENDING;
}

static bool signal_handle_jobctl_event(pcb_t pcb, irq_flags_t *irq_flag)
{
    pcb_t leader = pcb->group_leader;
    unsigned long jobctl = leader->jobctl;
    int signo = signal_ctrl_stop_signo(jobctl);

    if (jobctl & SIGNAL_CTRL_GROUP_STOP_PENDING)
    {
        /* group-stop 由接收侧统一完成通知父进程 + 停止线程组。 */
        leader->jobctl &= ~(SIGNAL_CTRL_GROUP_STOP_PENDING | SIGNAL_CTRL_STOP_SIG_MASK);

        if (!signo)
        {
            signo = SIGSTOP;
        }

        sighand_unlock(pcb, irq_flag);
        do_signal_stop(pcb, signo);
        sighand_lock(pcb, irq_flag);
        return true;
    }

    if (jobctl & SIGNAL_CTRL_CONTINUE_REPORT_PENDING)
    {
        /* continue 事件只在这里发布一次，避免发送侧和接收侧重复通知。 */
        leader->jobctl &= ~SIGNAL_CTRL_CONTINUE_REPORT_PENDING;

        sighand_unlock(pcb, irq_flag);
        signal_notify_parent_and_leader(leader, SIGCONT, false);
        sighand_lock(pcb, irq_flag);
        return true;
    }

    return false;
}

static int signal_collect_next_pending(pcb_t pcb,
                                       struct k_sigpending_queue **pending_queue,
                                       bool *is_private_signal,
                                       ksiginfo_entry_t *signal_entry)
{
    int signo;
    ksiginfo_entry_t *dequeued_entry;

    signo = peek_signal(pcb, pending_queue, is_private_signal);
    if (!signo)
    {
        return 0;
    }

    dequeued_entry = sigqueue_dequeue_single(*pending_queue, signo);
    if (!dequeued_entry)
    {
        return -EAGAIN;
    }

    *signal_entry = *dequeued_entry;
    siginfo_delete(dequeued_entry);

    return signo;
}

static bool signal_prepare_pending_entry(pcb_t pcb,
                                         int signo,
                                         bool is_private_signal,
                                         ksiginfo_entry_t *signal_entry)
{
    if (signo == SIGALRM && !is_private_signal)
    {
        /* itimer 只针对进程级 SIGALRM，因此共享信号出队后需要重新装填。 */
        itimer_rearm(pcb);
    }

    if (signal_entry->ksiginfo.si_code == SI_TIMER && signal_entry->priv)
    {
        if (!posixtimer_signal_process(signal_entry))
        {
            return false;
        }
    }

    return true;
}

static void signal_setup_user_handler(struct ksignal *ksig,
                                      struct k_sigaction *sigaction,
                                      ksiginfo_entry_t *signal_entry)
{
    ksig->ka = *sigaction;
    ksig->info = signal_entry->ksiginfo;

    if (sigaction->sa_flags & SA_ONESHOT)
    {
        sigaction->__sa_handler._sa_handler = SIG_DFL;
    }
}

static bool signal_handle_default_path(pcb_t pcb,
                                       int signo,
                                       struct k_sigaction *sigaction,
                                       ksiginfo_entry_t *signal_entry,
                                       irq_flags_t *irq_flag)
{
    if (signal_default_ignored(signo))
    {
        trace_signal_deliver(signal_entry->ksiginfo.si_code,
                             signal_entry->ksiginfo.si_signo,
                             sigaction->sa_flags);
        return true;
    }

    if (is_stop_set(signo))
    {
        /*
         * 默认 stop 动作先转换成 jobctl 控制事件，再由下一轮循环统一消费。
         * 这样 group-stop/continue 的时序会与普通 pending signal 分离开。
         */
        trace_signal_deliver(signal_entry->ksiginfo.si_code,
                             signal_entry->ksiginfo.si_signo,
                             sigaction->sa_flags);

        signal_mark_jobctl_stop(pcb, signo);
        return true;
    }

    sighand_unlock(pcb, irq_flag);

    trace_signal_deliver(signal_entry->ksiginfo.si_code,
                         signal_entry->ksiginfo.si_signo,
                         sigaction->sa_flags);

    exit_del(signo, pcb);

    sighand_lock(pcb, irq_flag);
    return true;
}

bool signal_fetch_deliverable(struct ksignal *ksig)
{
    int signo = 0;
    bool is_private_signal = false;
    struct k_sigaction *sigaction = NULL;
    irq_flags_t irq_flag = 0;
    pcb_t pcb = ttosProcessSelf();
    assert(!!pcb && !!pcb->taskControlId);
    ksiginfo_entry_t signal_entry = {0};
    struct k_sigpending_queue *pending_queue = NULL;

    sighand_lock(pcb, &irq_flag);

    /* 可以被信号打断 */
    if(!pcb_signal_pending(pcb))
    {
        sighand_unlock(pcb, &irq_flag);
        return false;
    }

    for (;;)
    {
        /*
         * 先处理 group-stop/continue
         * 这类控制事件，再处理普通 pending signal。
         */
        if (signal_handle_jobctl_event(pcb, &irq_flag))
        {
            continue;
        }

        signo = signal_collect_next_pending(pcb, &pending_queue, &is_private_signal, &signal_entry);
        if (!signo)
        {
            break;
        }

        if (signo < 0)
        {
            continue;
        }

        if (!signal_prepare_pending_entry(pcb, signo, is_private_signal, &signal_entry))
        {
            continue;
        }

        sigaction = sigaction_get_ptr(pcb, signo);

        if (sigaction_handler_get(sigaction) == SIG_IGN)
        {
            continue;
        }

        if (sigaction_handler_get(sigaction) != SIG_DFL)
        {
            signal_setup_user_handler(ksig, sigaction, &signal_entry);
            break;
        }

        if (signal_handle_default_path(pcb, signo, sigaction, &signal_entry, &irq_flag))
        {
            continue;
        }
    }

    /* 按统一规则重算当前线程的 pending 标记，不能只看 private queue 是否为空。 */
    signal_pending_recalc_current_locked();

    sighand_unlock(pcb, &irq_flag);

    ksig->sig = signo;
    return signo > 0;
}

void signal_delivered(struct ksignal *ksig, int stepping)
{
    k_sigset_t blocked = {0};
    pcb_t pcb = ttosProcessSelf();
    (void)stepping;

    pcb->need_restore_blocked = false;

    signal_set_or(&blocked, &pcb->blocked, (k_sigset_t *)&ksig->ka.mask);

    /* SA_NODEFER标志决定在调用handler期间是否要mask 该信号 */
    if (!(ksig->ka.sa_flags & SA_NODEFER))
    {
        signal_set_add(&blocked, ksig->sig);
    }

    signal_mask_apply_current(&blocked);

    if (pcb->altstack_flags & SS_AUTODISARM)
    {
        altstack_reset(pcb);
    }

    trace_signal_deliver(ksig->info.si_code, ksig->info.si_signo, ksig->ka.sa_flags);
}

bool in_syscall(struct arch_context *context)
{
    return (SYSCALL_CONTEXT == context->type) ? true : false;
}

void clear_syscall_context(struct arch_context *context)
{
    context->type = CONTEXT_NONE;
}
