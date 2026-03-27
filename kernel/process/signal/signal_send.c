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
#include <ipi.h>
#include <signal.h>
#include <ttos.h>
#include <ttosBase.h>
#include <ttosProcess.h>
#include <signal_internal.h>
#include <tasklist_lock.h>
#include <tglock.h>
#include <time/posix_timer.h>
#include <wqueue.h>
#include <trace.h>
#include <period_sched_group.h>

#undef KLOG_TAG
#define KLOG_TAG "signal"
#include <klog.h>

#undef KLOG_D
#define KLOG_D(...)

struct signal_work_param
{
    pid_t pid;
    int to;
    int signal;
    int si_code;
    bool has_kinfo;
    ksiginfo_entry_t kinfo_copy;
    struct work_s work;
};

struct signal_delivery_target
{
    pcb_t receiver;
    unsigned int cpu_index;
    bool should_notify;
};

/* ==================== 信号通知与投递 ==================== */

/**
 * tgroup_find_signal_thread_locked - 在线程组中选择信号接收线程
 * @pcb:   进程组内任意线程（用于定位 group_leader）
 * @signo: 信号编号
 *
 * 调用方必须已持有 tg_lock。
 * 优先选择未屏蔽该信号且优先级最高的线程；
 * 返回 NULL 表示线程组内所有线程均屏蔽该信号。
 */
static pcb_t tgroup_find_signal_thread_locked(pcb_t pcb, int signo)
{
    pcb_t catcher = NULL;
    pcb_t candidate;

    list_for_each_entry(candidate, &pcb->group_leader->thread_group, sibling_node)
    {
        if (!signal_set_contains(&candidate->blocked, signo))
        {
            if (!catcher ||
                candidate->taskControlId->taskCurPriority < catcher->taskControlId->taskCurPriority)
            {
                catcher = candidate;
            }
        }
    }

    return catcher;
}

static bool task_is_interruptible(T_TTOS_TaskControlBlock *tcb, unsigned int sig)
{
    unsigned int state = tcb->state;

    if ((sig == SIGKILL || sig == SIGSTOP) ||
        (state & TTOS_TASK_INTERRUPTIBLE_BY_SIGNAL) == TTOS_TASK_INTERRUPTIBLE_BY_SIGNAL)
    {
        return true;
    }
    else
    {
        return false;
    }
}

static void signal_wakeup_task(T_TTOS_TaskControlBlock *tcb, ksiginfo_entry_t *siginfo)
{
    int task_stat = tcb->state;

    /* 设置任务被信号中断标识 */
    tcb->wait.returnCode = TTOS_SIGNAL_INTR;

    memset(&tcb->wait.sig_info, 0, sizeof(tcb->wait.sig_info));

    /* 记录信号信息 */
    memcpy(&tcb->wait.sig_info, siginfo, sizeof(tcb->wait.sig_info));

    /* 任务在计时等待 */
    if (tcb->objCore.objectNode.next != NULL)
    {
        /* 将task任务从任务tick等待队列中移除 */
        ttosExactWaitedTask(tcb);
    }

    if (tcb->state & TTOS_KTIMER_SLEEP)
    {
        tcb->state &= ~(TTOS_KTIMER_SLEEP | TTOS_TASK_INTERRUPTIBLE_BY_SIGNAL);
        ttosSetTaskReady(tcb);
        return;
    }

    /* 将当前任务从其等待队列中移出 */
    ttosExtractTaskq(tcb->wait.queue, tcb);

    /* 被stop信号停止 */
    if (task_stat & TTOS_TASK_STOPPED_BY_SIGNAL)
    {
        TTOS_SignalResumeTask(tcb);
    }

    if (TTOS_TASK_SUSPEND == task_stat)
    {
        TTOS_ResumeTask(tcb);
    }

    /* 清除task任务的等待状态 */
    ttosClearTaskWaiting(tcb);
}

static void signal_notify_target(pcb_t pcb, ksiginfo_entry_t *siginfo)
{
    int sig = 0;
    irq_flags_t msr;
    unsigned int stat;
    T_TTOS_TaskControlBlock *tcb;

    if (!pcb)
    {
        return;
    }
    
    sig = siginfo->ksiginfo.si_signo;

    ttosDisableTaskDispatchWithLock();
    ttos_int_lock(msr);

    tcb = pcb->taskControlId;
    stat = tcb->state;

    /* 考虑非自启动的周期任务处理 */
    if (TTOS_SCHED_PERIOD == tcb->taskType)
    {
        if (stat == TTOS_TASK_DORMANT)
        {
            tcb->periodNode.delayTime = 0;
            period_thread_active_single(pcb);
            ttos_int_unlock(msr);
            ttosEnableTaskDispatchWithLock();
            return;
        }
    }

    if (TTOS_TASK_READY & stat)
    {
        ttos_int_unlock(msr);
        ttosEnableTaskDispatchWithLock();

        return;
    }

    if (signal_set_contains(&pcb->blocked, sig))
    {
        ttos_int_unlock(msr);
        ttosEnableTaskDispatchWithLock();
        return;
    }

    if (task_is_interruptible(tcb, sig))
    {
        signal_wakeup_task(tcb, siginfo);
        //KLOG_D("pcb:%p wakedup by signal %d", pcb, sig);
    }

    ttos_int_unlock(msr);
    ttosEnableTaskDispatchWithLock();
}

/* ==================== 信号发送 ==================== */

static void signal_release_unsent_info(long code, ksiginfo_t *kinfo)
{
    const ksiginfo_entry_t *entry;

    if (code != SI_CODE_POSIX_TIMER || !kinfo)
    {
        return;
    }

    /*
     * POSIX timer 在回调里先为“即将发送的这次通知”额外持有一份引用。
     * 如果发送在入队前就被跳过/失败，这份引用不会再经过正常 delivery 路径归还，
     * 因此要在发送侧失败路径主动释放。
     */
    entry = container_of(kinfo, const ksiginfo_entry_t, ksiginfo);
    posix_timer_signal_drop(entry->priv);
}

static int signal_send_prepare(pcb_t pcb, long signo)
{
    /* 僵尸进程不接收信号。 */
    if (pcb == NULL || pcb->exit_state == EXIT_ZOMBIE)
    {
        return -ESRCH;
    }

    /* signo == 0 只探测目标是否存在，不实际入队。 */
    if (signo == 0)
    {
        return 1;
    }

    if (!signal_num_valid(signo))
    {
        return -EINVAL;
    }

    /*
     * 发送侧只负责 signal_prepare_send() 这类“入队前”预处理：
     *   - stop/continue 互斥清理
     *   - continue 唤醒
     *   - 挂接后续由 signal_fetch_deliverable()/wait4 消费的控制事件
     * 真正的 stop/trap/continue 消费与上报仍留在接收侧。
     */
    if (!signal_prepare_send(signo, pcb, signo == SIGKILL))
    {
        return 1;
    }

    return 0;
}

static int signal_queue_to_process(pcb_t pcb,
                                   long signo,
                                   ksiginfo_entry_t *siginfo,
                                   struct signal_delivery_target *target)
{
    irq_flags_t sighand_flag = 0;
    irq_flags_t tg_flag = 0;
    pcb_t leader = pcb->group_leader;
    pcb_t receiver;

    tg_lock(leader, &tg_flag);

    receiver = tgroup_find_signal_thread_locked(pcb, signo);
    target->receiver = receiver;
    target->should_notify = receiver != NULL;

    /* 线程组共享队列和 shared sighand 都归 group leader 统一保护。 */
    sighand_lock(leader, &sighand_flag);
    sigqueue_enqueue(&signal_shared_get(leader)->sig_queue, siginfo);
    if (receiver)
    {
        pcb_signal_pending_set(receiver);
    }
    target->cpu_index = receiver ? receiver->taskControlId->smpInfo.cpuIndex : CPU_NONE;
    sighand_unlock(leader, &sighand_flag);

    tg_unlock(leader, &tg_flag);
    return 0;
}

static int signal_queue_to_thread(pcb_t pcb,
                                  ksiginfo_entry_t *siginfo,
                                  struct signal_delivery_target *target)
{
    irq_flags_t sighand_flag = 0;

    sighand_lock(pcb, &sighand_flag);

    sigqueue_enqueue(sigqueue_private_get(pcb), siginfo);
    pcb_signal_pending_set(pcb);

    target->receiver = pcb;
    target->cpu_index = pcb->taskControlId->smpInfo.cpuIndex;
    target->should_notify = true;

    sighand_unlock(pcb, &sighand_flag);
    return 0;
}

static void signal_publish_queued_signal(const struct signal_delivery_target *target,
                                         ksiginfo_entry_t *siginfo)
{
    ksiginfo_entry_t notify_siginfo;

    if (!target->should_notify)
    {
        return;
    }

    /*
     * 用栈上的副本做唤醒通知，避免队列里的节点在其他 CPU 上被消费后失效。
     * 共享队列里如果所有线程当前都屏蔽该信号，这里也不主动唤醒，等接收方解除屏蔽后再处理。
     */
    notify_siginfo = *siginfo;
    signal_notify_target(target->receiver, &notify_siginfo);
    signal_cpu_notify(target->cpu_index);
}

/**
 * signal_send - 向目标线程/进程投递信号（核心发送路径）
 * @pcb:   目标 PCB（TO_PROCESS 时为进程内任意线程，TO_THREAD 时为精确线程）
 * @signo: 信号编号
 * @code:  si_code
 * @kinfo: 附加信号信息（可为 NULL）
 * @flag:  TO_PROCESS 或 TO_THREAD
 *
 * 加锁语义：
 *   TO_PROCESS：tg_lock → sighand_lock（在 tg_lock 内选线程，保证 dest_pcb 在整个
 *               入队过程中不会退出线程组；sighand_lock 内完成入队与 pending flag 置位）
 *   TO_THREAD ：sighand_lock（入队与 pending flag 置位均在锁内完成）
 *   signal_notify_target / signal_cpu_notify 在所有锁释放后调用，避免持锁期间做调度操作。
 */
static int signal_send(pcb_t pcb, long signo, long code, ksiginfo_t *kinfo, int flag)
{
    ksiginfo_entry_t *siginfo = NULL;
    struct signal_delivery_target target = {0};
    int ret;

    ret = signal_send_prepare(pcb, signo);
    if (ret != 0)
    {
        /* 正值表示“无需继续发送，但也不是错误”，例如 signo==0 或发送侧已判定可跳过。 */
        signal_release_unsent_info(code, kinfo);
        return ret > 0 ? 0 : ret;
    }

    /* siginfo 在持锁前分配，避免持锁期间 malloc */
    siginfo = siginfo_create(signo, code, kinfo);
    if (!siginfo)
    {
        KLOG_E("%s: siginfo malloc failed", __func__);
        signal_release_unsent_info(code, kinfo);
        return -ENOMEM;
    }

    if (flag == TO_PROCESS)
    {
        ret = signal_queue_to_process(pcb, signo, siginfo, &target);
    }
    else if (flag == TO_THREAD)
    {
        ret = signal_queue_to_thread(pcb, siginfo, &target);
    }
    else
    {
        KLOG_E("%s: invalid flag %d", __func__, flag);
        siginfo_delete(siginfo);
        signal_release_unsent_info(code, kinfo);
        return -EINVAL;
    }

    if (ret)
    {
        siginfo_delete(siginfo);
        signal_release_unsent_info(code, kinfo);
        return ret;
    }

    signal_publish_queued_signal(&target, siginfo);
    return 0;
}

static int do_process_signal_send(pid_t pid, long signo, long si_code, ksiginfo_t *kinfo)
{
    int ret = 0;
    pcb_t pcb;

    pcb = pcb_get_by_pid(pid);

    if (pcb && 0 == signo)
    {
        return 0;
    }

    if (pcb && pcb_get(pcb))
    {
        ret = signal_send(pcb, signo, si_code, kinfo, TO_PROCESS);
        pcb_put(pcb);
        return ret;
    }

    return -ESRCH;
}

static int do_kernel_send(int tid, int signo, long si_code, ksiginfo_t *kinfo)
{
    int ret = 0;
    T_TTOS_TaskControlBlock *thread;

    thread = task_get_by_tid(tid);

    if (thread && 0 == signo)
    {
        return 0;
    }

    if (thread && thread->ppcb && pcb_get(thread->ppcb))
    {
        ret = signal_send(thread->ppcb, signo, si_code, kinfo, TO_THREAD);
        pcb_put(thread->ppcb);
        return ret;
    }

    return -ESRCH;
}

static int do_pgroup_signal_send(pid_t pgid, long signo, long si_code, ksiginfo_t *kinfo)
{
    int ret = 0;
    pgroup_t pgrp;
    pcb_t pcb;

    tasklist_lock();

    pgrp = process_pgrp_find(pgid);

    if (pgrp && 0 == signo)
    {
        pgrp_put(pgrp);
        tasklist_unlock();
        return 0;
    }

    if (pgrp == NULL)
    {
        tasklist_unlock();
        return -ESRCH;
    }

    if (pgrp)
    {
        TTOS_ObtainMutex(pgrp->lock, TTOS_WAIT_FOREVER);
        list_for_each_entry(pcb, &pgrp->process, pgrp_node)
        {
            pid_t pid = get_process_pid(pcb);
            ret = do_process_signal_send(pid, signo, si_code, kinfo);
            trace_signal_generate(si_code, pgid, pid, signo, ret);
        }
        TTOS_ReleaseMutex(pgrp->lock);
        pgrp_put(pgrp);
        ret = 0;
    }

    tasklist_unlock();
    return ret;
}

static int do_kernel_signal_send(pid_t pid, int to, long signo, long code, ksiginfo_t *kinfo)
{
    int ret;
    switch (to)
    {
        case TO_PROCESS:
            ret = do_process_signal_send(pid, signo, code, kinfo);
            break;
        case TO_THREAD:
            ret = do_kernel_send(pid, signo, code, kinfo);
            break;
        case TO_PGROUP:
            ret = do_pgroup_signal_send(pid, signo, code, kinfo);
            break;
        default:
            KLOG_EMERG("%s: invalid to %d", __func__, to);
            ret = -EINVAL;
    }

    if (signo && ((to == TO_PROCESS) || (to == TO_THREAD)))
    {
        trace_signal_generate(code, 0, pid, signo, ret);
    }
    return ret;
}

static void do_kernel_send_work(void *arg)
{
    struct signal_work_param *param = (struct signal_work_param *)arg;
    ksiginfo_t *kinfo = param->has_kinfo ? &param->kinfo_copy.ksiginfo : NULL;

    do_kernel_signal_send(param->pid, param->to, param->signal, param->si_code, kinfo);
    free(param);
}

int kernel_signal_send_with_worker(pid_t pid, int to, long signo, long code, ksiginfo_t *kinfo)
{
    struct signal_work_param *param = malloc(sizeof(struct signal_work_param));
    int ret;

    if (param == NULL)
    {
        KLOG_E("malloc failed at %s %d", __func__, __LINE__);
        signal_release_unsent_info(code, kinfo);
        return -ENOMEM;
    }
    param->pid = pid;
    param->to = to;
    param->signal = signo;
    param->si_code = code;

    /*
     * worker 发送会跨出当前中断/idle 上下文。
     * 这里必须把 kinfo 复制到 worker 自己的存储里，避免回调返回后原始栈对象、
     * 或者 POSIX timer 自带的 siginfo 节点提前失效。
     */
    param->has_kinfo = kinfo != NULL;
    memset(&param->kinfo_copy, 0, sizeof(param->kinfo_copy));
    if (kinfo)
    {
        memcpy(&param->kinfo_copy.ksiginfo, kinfo, sizeof(*kinfo));

        /*
         * SI_CODE_POSIX_TIMER 还需要把附着在 siginfo->priv 上的 timer 指针一起带过去，
         * 这样 worker 里的 siginfo_create() 才能构造出完整的 queued signal。
         */
        if (code == SI_CODE_POSIX_TIMER)
        {
            const ksiginfo_entry_t *entry = container_of(kinfo, const ksiginfo_entry_t, ksiginfo);
            param->kinfo_copy.priv = entry->priv;
        }
    }

    ret = work_queue(&param->work, (worker_t)do_kernel_send_work, param, 0,
                     work_queue_get_highpri());
    if (ret)
    {
        /*
         * worker 如果没能真正入队，这次异步发送就不会再有后续回调。
         * 因此这里要把 worker 私有参数和未入队 signal 持有的外部引用一起回收。
         */
        free(param);
        signal_release_unsent_info(code, kinfo);
        return ret;
    }

    /* 在中断中调用的话 一定不是用户态主动调的 不用考虑获取返回值 */
    return 0;
}

int kernel_signal_send(pid_t pid, int to, long signo, long code, ksiginfo_t *kinfo)
{
    int ret = -ESRCH;

    /* 如果在中断/idle中 则使用worker发送 */
    if (ttosIsISR() || ttosIsIdleTask(ttosGetRunningTask()))
    {
        return kernel_signal_send_with_worker(pid, to, signo, code, kinfo);
    }

    ret = do_kernel_signal_send(pid, to, signo, code, kinfo);

    return ret;
}

int kernel_sigqueueinfo(pid_t pid, int sig, siginfo_t *kinfo)
{
    pcb_t pcb;

    if ((kinfo->si_code >= 0 || kinfo->si_code == SI_TKILL) &&
        (get_process_pid(ttosProcessSelf()) != pid))
    {
        return -EPERM;
    }

    return kernel_signal_send(pid, TO_PROCESS, sig, kinfo->si_code, kinfo);
}

void process_signal_kill_to(pcb_t pcb, long sig)
{
    pid_t pid;
    if (pcb != NULL)
    {
        pid = get_process_pid(pcb);
        if (pid != 1)
        {
            kernel_signal_send(pid, TO_PROCESS, sig, SI_USER, 0);
        }
    }
}
