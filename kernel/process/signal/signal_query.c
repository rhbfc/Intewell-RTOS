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
#include <signal_internal.h>
#include <tglock.h>
#include <ttos.h>
#include <ttosBase.h>
#include <ttosProcess.h>
#include <ttos_pic.h>

#undef KLOG_TAG
#define KLOG_TAG "signal"
#include <klog.h>

#undef KLOG_D
#define KLOG_D(...)

unsigned long ttos_ffz(unsigned long x);

/* ==================== 信号查找 ==================== */

int signal_find_next_pending(k_sigset_t *pending, k_sigset_t *mask);

/* ==================== 信号分类 ==================== */

bool signal_default_ignored(int signo)
{
    k_sigset_t ign_set = signal_set_from_long_mask(PROCESS_SIG_IGNORE_SET);
    return signal_set_contains(&ign_set, signo);
}

bool signal_send_ignored(pcb_t pcb, int signo)
{
    sighandler_t action;
    k_sigset_t ign_set = signal_set_from_long_mask(PROCESS_SIG_IGNORE_SET);

    action = sighandler_get(pcb, signo);

    /*
     * 这里只回答“当前这条信号在发送侧是否可以直接跳过”：
     *   - 已经被当前线程屏蔽的信号不能算忽略，它们只是延后处理
     */
    if (signal_set_contains(&pcb->blocked, signo) || signal_set_contains(&pcb->real_blocked, signo))
    {
        return false;
    }

    if (action == SIG_ACT_IGN)
    {
        return true;
    }
    else if (action == SIG_ACT_DFL && signal_set_contains(&ign_set, signo))
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool is_rt_signo(int signo)
{
    return (signo >= SIGRTMIN && signo <= SIGRTMAX) ? true : false;
}

bool is_stop_set(int signo)
{
    k_sigset_t stop_sigset = signal_set_from_long_mask(PROCESS_SIG_STOP_SET);

    return signal_set_contains(&stop_sigset, signo);
}

bool sig_immutable(int signo)
{
    return signo == SIGKILL || signo == SIGSTOP;
}

bool signal_num_valid(int sig)
{
    return (sig >= 1 && sig <= SIGNAL_MAX);
}

bool is_thread_group_leader(pcb_t p)
{
    return (p->group_leader == p);
}

static bool signal_group_exit_in_progress(pcb_t pcb)
{
    pcb_t leader;

    if (!pcb)
    {
        return false;
    }

    leader = pcb->group_leader;
    return leader->group_exit_status.is_terminated ||
           leader->group_exit_status.group_request_terminate;
}

static void signal_prepare_stop(pcb_t pcb)
{
    k_sigset_t remove_sigset;
    pcb_t leader = pcb->group_leader;

    /* 发送 stop 类信号前，先清掉之前排队但尚未消费的 SIGCONT。 */
    remove_sigset = signal_set_from_long_mask(signal_long_mask(SIGCONT));
    sigqueue_remove_shared(pcb, &remove_sigset);
    sigqueue_remove_private(pcb, &remove_sigset);

    /* 旧的 continue 事件还没被接收侧消费时，新的 stop 应当把它作废。 */
    leader->jobctl &= ~SIGNAL_CTRL_CONTINUE_REPORT_PENDING;
}

static void signal_resume_thread_group(pcb_t pcb)
{
    pcb_t tmp_pcb = NULL;
    irq_flags_t irq_flags;
    bool is_locked;

    is_locked = tg_lock_is_held(pcb->group_leader);
    if (!is_locked)
    {
        tg_lock(pcb->group_leader, &irq_flags);
    }

    for_each_thread_in_tgroup(pcb, tmp_pcb)
    {
        /* 发送侧只负责恢复运行条件，后续 continue 上报留给 signal_fetch_deliverable()/wait4。 */
        TTOS_SignalResumeTask(tmp_pcb->taskControlId);
    }

    if (!is_locked)
    {
        tg_unlock(pcb->group_leader, &irq_flags);
    }
}

static void signal_prepare_continue(pcb_t pcb)
{
    k_sigset_t remove_sigset;
    pcb_t leader = pcb->group_leader;
    bool was_group_stopped;

    was_group_stopped = !!leader->jobctl_stopped;

    leader->jobctl_stopped = false;
    pcb->jobctl_stopped = false;
    leader->jobctl &= ~(SIGNAL_CTRL_GROUP_STOP_PENDING | SIGNAL_CTRL_STOP_SIG_MASK);

    if (was_group_stopped)
    {
        /* 只有线程组真的处于 stopped 状态时，才挂一个后续待发布的 continue 事件。 */
        leader->jobctl |= SIGNAL_CTRL_CONTINUE_REPORT_PENDING;
    }

    /* SIGCONT 到达时，清掉所有 stop 类 pending。 */
    remove_sigset = signal_set_from_long_mask(PROCESS_SIG_STOP_SET);
    sigqueue_remove_shared(pcb, &remove_sigset);
    sigqueue_remove_private(pcb, &remove_sigset);

    signal_resume_thread_group(pcb);
}

/* ==================== 信号查找 ==================== */

int signal_find_next_pending(k_sigset_t *pending, k_sigset_t *mask)
{
    unsigned long word_index;
    unsigned long *pending_words;
    unsigned long *mask_words;
    unsigned long unmasked_bits;
    int signo = 0;

    pending_words = pending->sig;
    mask_words = mask->sig;

    /*
     * 先计算 pending & ~mask：
     *   - pending 表示当前已经挂起的信号
     *   - mask 表示当前被阻塞的信号
     * 只有“已挂起且未被阻塞”的信号才有资格被返回。
     */
    unmasked_bits = *pending_words & ~*mask_words;
    if (unmasked_bits)
    {
        signo = ttos_ffz(~unmasked_bits) + 1;
        return signo;
    }

    switch (K_SIGSET_NLONG)
    {
    case 1:
        /* 整个信号集只占 1 个 word，第 0 个 word 已经检查完毕。 */
        break;

    case 2:
        /* 32 位平台下，64 个信号会拆成 2 个 word，这里检查第 2 个 word。 */
        unmasked_bits = pending_words[1] & ~mask_words[1];
        if (!unmasked_bits)
        {
            break;
        }

        signo = ttos_ffz(~unmasked_bits) + BITS_PER_LONG + 1;
        break;

    default:
        /*
         * 更大信号集的通用扫描逻辑：
         * 从第 2 个 word 开始，按 word 顺序查找第一个“已挂起且未被阻塞”的信号。
         */
        for (word_index = 1; word_index < K_SIGSET_NLONG; ++word_index)
        {
            unmasked_bits = *++pending_words & ~*++mask_words;
            if (!unmasked_bits)
            {
                continue;
            }

            signo = ttos_ffz(~unmasked_bits) + word_index * BITS_PER_LONG + 1;
            break;
        }
        break;
    }

    return signo;
}

int sigqueue_peek(struct k_sigpending_queue *sigqueue, k_sigset_t *mask)
{
    return signal_find_next_pending(&sigqueue->sigset_pending, mask);
}

/* ==================== 信号状态查询 ==================== */

bool signal_prepare_send(int sig, pcb_t pcb, bool force)
{
    /*
     * signal_prepare_send() 只处理发送前副作用：
     *   - group-exit 短路
     *   - stop/continue 的互斥清理
     *   - 挂接后续由 signal_fetch_deliverable()/wait4 发布的控制事件
     * 它不负责真正消费 stop/continue，也不直接替 wait4 产出最终状态。
     */
    if (signal_group_exit_in_progress(pcb))
    {
        return force;
    }

    if (is_stop_set(sig))
    {
        signal_prepare_stop(pcb);
    }
    else if (sig == SIGCONT)
    {
        signal_prepare_continue(pcb);
    }

    if (force)
    {
        return true;
    }

    /* 检测信号是否是被忽略的 */
    return !signal_send_ignored(pcb, sig);
}

bool signal_should_queue(int sig, pcb_t pcb)
{
    return signal_prepare_send(sig, pcb, false);
}

/* 获取已经存在并且被阻塞的信号集合 */
void kernel_signal_pending(pcb_t pcb, k_sigset_t *pending)
{
    irq_flags_t flag;

    if (pcb)
    {
        sighand_lock(pcb, &flag);

        signal_set_or(pending, sigqueue_private_pending_get(pcb), sigqueue_shared_pending_get(pcb));

        signal_set_and(pending, pending, &pcb->blocked);

        sighand_unlock(pcb, &flag);
    }
}

bool pending_signal_exist(void)
{
    k_sigset_t pending;
    k_sigset_t pending_null;

    memset(&pending, 0, sizeof(k_sigset_t));
    memset(&pending_null, 0, sizeof(k_sigset_t));

    kernel_signal_pending(ttosProcessSelf(), &pending);

    if (memcmp(&pending_null, &pending, sizeof(k_sigset_t)))
    {
        return true;
    }
    else
    {
        return false;
    }
}

void signal_cpu_notify(unsigned int cpu_index)
{
    if (cpu_index != CPU_NONE && cpu_index != ttosGetRunningTask()->smpInfo.cpuIndex)
    {
        ttos_pic_ipi_send(GENERAL_IPI_SCHED, cpu_index, 0);
    }
}

bool signal_should_report_status(pcb_t pcb, int signo)
{
    struct shared_signal *signal = signal_shared_get(pcb);
    {
        k_sigset_t jobctl_set = signal_set_from_long_mask(PROCESS_SIG_JOBCTL_SET);
        assert(signal_set_contains(&jobctl_set, signo));
    }
    return !signal_set_contains(&signal->sigchld_nostop_set, SIGCHLD);
}
