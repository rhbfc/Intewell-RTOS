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
#include <signal.h>
#include <ttos.h>
#include <ttosBase.h>
#include <ttosProcess.h>
#include <signal_internal.h>
#include <tglock.h>
#include <time/posix_timer.h>
#include <time/itimer.h>

#undef KLOG_TAG
#define KLOG_TAG "signal"
#include <klog.h>

#undef KLOG_D
#define KLOG_D(...)

/* ==================== 锁操作 ==================== */

void sighand_lock (pcb_t pcb, irq_flags_t *irq_flag)
{
    irq_flags_t flag;
    struct shared_sighand *sighand = NULL;

    if (!pcb)
    {
        return;
    }

    sighand = sighand_shared_get(pcb);

    spin_lock_irqsave(&sighand->siglock, flag);

    *irq_flag = flag;
}

void sighand_unlock(pcb_t pcb, irq_flags_t *irq_flag)
{
    irq_flags_t flag = *irq_flag;
    struct shared_sighand *sighand = NULL;

    if (!pcb)
    {
        return;
    }

    sighand = sighand_shared_get(pcb);

    spin_unlock_irqrestore(&sighand->siglock, flag);
}

void signal_lock(pcb_t pcb, irq_flags_t *irq_flag)
{
    irq_flags_t flag;
    struct shared_signal *signal = NULL;

    assert(!!pcb);

    signal = signal_shared_get(pcb);

    spin_lock_irqsave(&signal->siglock, flag);

    *irq_flag = flag;
}

void signal_unlock(pcb_t pcb, irq_flags_t *irq_flag)
{
    irq_flags_t flag = *irq_flag;
    struct shared_signal *signal = NULL;

    assert(!!pcb);

    signal = signal_shared_get(pcb);

    spin_unlock_irqrestore(&signal->siglock, flag);
}

/* ==================== 访问器 ==================== */

struct shared_sighand *sighand_shared_get(pcb_t pcb)
{
    return get_process_sighand(pcb);
}

struct k_sigaction *sigaction_get_ptr (pcb_t pcb, int signo)
{
    return (struct k_sigaction *)&sighand_shared_get(pcb)->sig_action[signo - 1];
}

sighandler_t sighandler_get(pcb_t pcb, int signo)
{
    return sighand_shared_get(pcb)->sig_action[signo - 1].__sa_handler._sa_handler;
}

struct shared_signal *signal_shared_get(pcb_t pcb)
{
    struct shared_signal *signal = get_process_signal(pcb);
    return signal;
}

struct k_sigpending_queue *sigqueue_private_get(pcb_t pcb)
{
    return &pcb->sig_queue;
}

struct k_sigpending_queue *sigqueue_shared_get(pcb_t pcb)
{
    return &signal_shared_get(pcb)->sig_queue;
}

k_sigset_t *sigqueue_private_pending_get(pcb_t pcb)
{
    return &sigqueue_private_get(pcb)->sigset_pending;
}

k_sigset_t *sigqueue_shared_pending_get(pcb_t pcb)
{
    return &sigqueue_shared_get(pcb)->sigset_pending;
}

/* ==================== 信号队列操作 ==================== */

int sigqueue_isempty(struct k_sigpending_queue *sigqueue)
{
    return signal_set_is_empty(&sigqueue->sigset_pending);
}

ksiginfo_entry_t *siginfo_create(int signo, int code, ksiginfo_t *info)
{
    ksiginfo_entry_t *siginfo = NULL;
    pcb_t cur_pcb;

    siginfo = calloc(1, sizeof(ksiginfo_entry_t));
    if (siginfo)
    {
        if (info)
        {
            memcpy(&siginfo->ksiginfo, info, sizeof(siginfo->ksiginfo));
        }

        if (SI_CODE_POSIX_TIMER == code)
        {
            code = SI_TIMER;
            ksiginfo_entry_t *tmp_info = container_of(info, ksiginfo_entry_t, ksiginfo);
            siginfo->priv              = tmp_info->priv;
        }

        cur_pcb = ttosProcessSelf();
        if (cur_pcb && (code == SI_USER))
        {
            siginfo->ksiginfo.si_pid = get_process_pid(cur_pcb);
            siginfo->ksiginfo.si_uid = 0;
        }

        siginfo->ksiginfo.si_signo = signo;
        siginfo->ksiginfo.si_code  = code;
    }

    return siginfo;
}

void siginfo_delete(ksiginfo_entry_t *siginfo)
{
    if (siginfo)
    {
        /*
         * siginfo_delete() 只负责释放 siginfo 节点自身。
         * 诸如 POSIX timer 这类挂在 priv 上的外部对象，需要在调用方明确决定是否归还引用。
         */
        free(siginfo);
    }
}

static void siginfo_release_dropped(ksiginfo_entry_t *siginfo)
{
    if (!siginfo)
    {
        return;
    }

    /*
     * 这类节点不会再进入 signal delivery 流程，自然也不会走到
     * posixtimer_signal_process() 去归还 timer 回调持有的额外引用。
     * 因此在丢弃 queued signal 时要把这份引用在这里补还。
     */
    if (siginfo->ksiginfo.si_code == SI_TIMER && siginfo->priv)
    {
        posix_timer_signal_drop(siginfo->priv);
    }

    siginfo_delete(siginfo);
}

int sigqueue_ismember(struct k_sigpending_queue *sigqueue, int signo)
{
    return signal_set_contains(&sigqueue->sigset_pending, signo);
}

void sigqueue_enqueue(struct k_sigpending_queue *sigqueue, ksiginfo_entry_t *siginfo)
{
    bool inserted = false;

    /*
     * pending 位图负责选择“哪个 signo 可以先被看到”，链表只保存该 signo 对应的
     * 具体 siginfo 负载，因此这里不额外按 signo 排序；新节点直接挂到链表前部。
     */
    if (!list_is_empty(&sigqueue->siginfo_list))
    {
        list_add_after(&siginfo->node, &sigqueue->siginfo_list);
        inserted = true;
    }

    /* 空链表时，前后插入等价；统一把首个节点挂到表头。 */
    if (!inserted)
    {
        list_add_before(&siginfo->node, &sigqueue->siginfo_list);
    }

    /* 加入pending信号集 */
    signal_set_add(&sigqueue->sigset_pending, siginfo->ksiginfo.si_signo);
}

ksiginfo_entry_t *sigqueue_dequeue_single(struct k_sigpending_queue *sigqueue, int signo)
{
    ksiginfo_entry_t *found = NULL;
    ksiginfo_entry_t *candidate;
    ksiginfo_entry_t *next;
    bool has_more_same_signo = false;

    list_for_each_entry_safe(candidate, next, &sigqueue->siginfo_list, node)
    {
        if (candidate->ksiginfo.si_signo != signo)
        {
            continue;
        }

        if (!found)
        {
            found = candidate;
            list_del(&found->node);
            continue;
        }

        has_more_same_signo = true;
        break;
    }

    /*
     * 只有当同 signo 的最后一个节点被移走时，才清除 pending 位。
     * 这样下一轮扫描仍能看到剩余的同 signo 信号。
     */
    if (found && !has_more_same_signo)
    {
        signal_set_del(&sigqueue->sigset_pending, signo);
    }

    return found;
}

void sigqueue_discard_sigset(struct k_sigpending_queue *sigqueue, k_sigset_t *sigset)
{
    ksiginfo_entry_t *queuing_si;
    k_sigset_t mask;
    int signo;

    /* sigqueue_peek中会再次取反 */
    signal_set_not(&mask, sigset);
    while ((signo = sigqueue_peek(sigqueue, &mask)) != 0)
    {
        queuing_si = sigqueue_dequeue_single(sigqueue, signo);
        siginfo_release_dropped(queuing_si);
    }
}

void sigqueue_discard(struct k_sigpending_queue * sigqueue, int signo)
{
    ksiginfo_entry_t *queuing_si;
    while (sigqueue_ismember(sigqueue, signo))
    {
        queuing_si = sigqueue_dequeue_single(sigqueue, signo);
        siginfo_release_dropped(queuing_si);
    }
}

void sigqueue_purge(struct k_sigpending_queue *sigqueue)
{
    ksiginfo_entry_t *cur;
    ksiginfo_entry_t *next;

    if (!sigqueue)
    {
        return;
    }

    /*
     * 用于“整队列清空”场景：
     *   - exec/reset 时放弃所有 pending signal
     *   - shared_signal 最终析构时收口所有残留节点
     * 这类路径不会再进入正常 delivery，因此每个节点都要按 dropped 语义释放。
     */
    list_for_each_entry_safe(cur, next, &sigqueue->siginfo_list, node)
    {
        list_del(&cur->node);
        siginfo_release_dropped(cur);
    }

    memset(&sigqueue->sigset_pending, 0, sizeof(sigqueue->sigset_pending));
    INIT_LIST_HEAD(&sigqueue->siginfo_list);
}

void signal_private_pending_flush(pcb_t pcb)
{
    irq_flags_t irq_flag = 0;

    /*
     * private pending queue 跟随单个 pcb 生命周期。
     * 在线程退出这类“直接放弃私有 pending”路径上，需要整队列清空，
     * 同时清掉线程级 SIGPENDING 标记，避免残留的 SI_TIMER 节点继续挂住 timer 引用。
     */
    sighand_lock(pcb, &irq_flag);
    sigqueue_purge(sigqueue_private_get(pcb));
    pcb_signal_pending_clear(pcb);
    sighand_unlock(pcb, &irq_flag);
}

void sigqueue_remove(pcb_t pcb, k_sigset_t *remove_sigset, struct k_sigpending_queue *sigqueue)
{
    ksiginfo_entry_t *cur;
    ksiginfo_entry_t *next;
    k_sigset_t intersection;
    irq_flags_t irq_flag = 0;

    /*
     * 这里内部自取 sighand_lock，调用方不需要预先持锁。
     * shared/private 两类 remove helper 都沿用这个约束。
     */
    sighand_lock(pcb, &irq_flag);

    signal_set_and(&intersection, remove_sigset, &sigqueue->sigset_pending);

    if (signal_set_is_empty(&intersection))
    {
        sighand_unlock(pcb, &irq_flag);
        return;
    }

    signal_set_and_not(&sigqueue->sigset_pending, &sigqueue->sigset_pending, remove_sigset);

    list_for_each_entry_safe(cur, next, &sigqueue->siginfo_list, node)
    {
        if (signal_set_contains(remove_sigset, cur->ksiginfo.si_signo))
        {
            list_del(&cur->node);
            siginfo_release_dropped(cur);
        }
    }

    sighand_unlock(pcb, &irq_flag);
}

void sigqueue_remove_shared(pcb_t pcb, k_sigset_t *remove_sigset)
{
    struct shared_signal *signal = signal_shared_get(pcb);

    sigqueue_remove(pcb, remove_sigset, &signal->sig_queue);
}

void sigqueue_remove_private(pcb_t pcb, k_sigset_t *remove_sigset)
{
    pcb_t tmp = NULL;
    irq_flags_t irq_flags;
    bool is_locked = false;

    if (!pcb)
    {
        return;
    }

    is_locked = tg_lock_is_held (pcb->group_leader);

    if (!is_locked)
    {
        tg_lock(pcb->group_leader, &irq_flags);
    }

    /* 私有 pending 分散在各线程上，因此需要在 tg_lock 下逐线程处理。 */
    for_each_thread_in_tgroup(pcb, tmp)
    {
        sigqueue_remove(tmp, remove_sigset, &tmp->sig_queue);
    }

    if (!is_locked)
    {
        tg_unlock(pcb->group_leader, &irq_flags);
    }
}


void process_sigqueue_init(struct k_sigpending_queue * sigq)
{
    memset(&sigq->sigset_pending, 0, sizeof(k_sigset_t));
    INIT_LIST_HEAD(&sigq->siginfo_list);
}

int sigqueue_examine(struct k_sigpending_queue * sigqueue, k_sigset_t *pending)
{
    int is_empty = sigqueue_isempty(sigqueue);
    if (!is_empty)
    {
        signal_set_or(pending, &sigqueue->sigset_pending, &sigqueue->sigset_pending);
    }
    return is_empty;
}

int signal_dequeue(pcb_t pcb, k_sigset_t *mask, siginfo_t *uinfo)
{
    int signo = 0;
    irq_flags_t irq_flag = 0;
    ksiginfo_entry_t *entry;
    ksiginfo_entry_t info = {0};
    k_sigset_t *pending;
    struct k_sigpending_queue *sigqueue;

retry:
    sighand_lock(pcb, &irq_flag);

    sigqueue = sigqueue_private_get(pcb);
    pending = &sigqueue->sigset_pending;
    signo = signal_find_next_pending(pending, mask);
    if (!signo)
    {
        struct shared_signal *signal = signal_shared_get(pcb);
        sigqueue = &signal->sig_queue;
        pending = &sigqueue->sigset_pending;
        signo = signal_find_next_pending(pending, mask);
    }

    if (signo)
    {
        entry = sigqueue_dequeue_single(sigqueue, signo);
        if (!entry)
        {
            sighand_unlock(pcb, &irq_flag);
            goto retry;
        }

        info = *entry;
        sighand_unlock(pcb, &irq_flag);

        if (uinfo)
        {
            memcpy(uinfo, &entry->ksiginfo, sizeof(*uinfo));
        }

        siginfo_delete(entry);

        if (signo == SIGALRM)
        {
            itimer_rearm_locked(pcb);
        }

        /* 是posix timer发出的信号 */
        if (info.ksiginfo.si_code == SI_TIMER && info.priv)
        {
            /* 检查posix timer信号是否需要被发出,posix timer是否需要重启 */
            if (!posixtimer_signal_process(&info))
            {
                /* 丢弃本次timer信号 */
                goto retry;
            }
        }
    }
    else
    {
        sighand_unlock(pcb, &irq_flag);
    }

    return signo;
}

void pcb_signal_pending_set(pcb_t pcb)
{
    pcb_flags_set(pcb, SIGPENDING);
}

void pcb_signal_pending_clear(pcb_t pcb)
{
    pcb_flags_clear(pcb, SIGPENDING);
}

bool pcb_signal_pending(pcb_t pcb)
{
    return pcb_flags_test(pcb, SIGPENDING);
}

static bool signal_has_control_event(pcb_t pcb)
{
    unsigned long jobctl;

    if (!pcb)
    {
        return false;
    }

    jobctl = pcb->group_leader->jobctl;
    return !!(jobctl & (SIGNAL_CTRL_GROUP_STOP_PENDING |
                        SIGNAL_CTRL_CONTINUE_REPORT_PENDING));
}

static bool signal_pending_recalc_locked(pcb_t pcb)
{
    k_sigset_t pending;
    k_sigset_t unblocked;

    if (signal_has_control_event(pcb))
    {
        pcb_signal_pending_set(pcb);
        return true;
    }

    /* 合并私有与共享 pending 集合，取出未被屏蔽的部分 */
    signal_set_or(&pending, sigqueue_private_pending_get(pcb), sigqueue_shared_pending_get(pcb));
    signal_set_not(&unblocked, &pcb->blocked);
    signal_set_and(&pending, &pending, &unblocked);

    if (!signal_set_is_empty(&pending))
    {
        pcb_signal_pending_set(pcb);
        return true;
    }

    /*
     * 不在此处清除 flag：其他线程调用时不可清除当前线程的 flag，
     * 当前线程若正从系统调用返回 -ERESTART* 也不应清除。
     * 由调用方决定是否清除。
     */
    return false;
}

void signal_pending_recalc_current_locked(void)
{
    pcb_t pcb = ttosProcessSelf();

    if (!pcb)
    {
        return;
    }

    if (!signal_pending_recalc_locked(pcb))
    {
        /* 无可投递信号：若 flag 仍置位则清除，保持状态一致 */
        if (pcb_signal_pending(pcb))
        {
            pcb_signal_pending_clear(pcb);
        }
    }
}

void signal_pending_recalc_current(void)
{
    pcb_t pcb = ttosProcessSelf();
    irq_flags_t irq_flag = 0;

    if (!pcb)
    {
        return;
    }

    sighand_lock(pcb, &irq_flag);
    signal_pending_recalc_current_locked();
    sighand_unlock(pcb, &irq_flag);
}
