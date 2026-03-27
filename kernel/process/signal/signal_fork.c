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
#include <time/kitimer.h>
#include <time/posix_timer.h>
#include <ttosRBTree.h>

#undef KLOG_TAG
#define KLOG_TAG "signal"
#include <klog.h>

enum ktimer_restart itimer_handler(struct ktimer *timer);

/* ==================== 进程组辅助 ==================== */

pid_t process_pgid_get_byprocess(pcb_t process)
{
    return process ? process->pgid : 0;
}

/* ==================== sighand 对象 ==================== */

static void sighand_obj_destroy(void *args)
{
    free(args);
}

struct process_obj *sighand_obj_alloc(pcb_t pcb)
{
    struct shared_sighand *sighandl;

    sighandl = calloc(1, sizeof(struct shared_sighand));

    spin_lock_init(&sighandl->siglock);

    return process_obj_create(pcb, sighandl, sighand_obj_destroy);
}

int sighand_obj_ref(pcb_t parent, pcb_t child)
{
    child->sighand = process_obj_ref(child, parent->sighand);
    return 0;
}

/* ==================== signal 对象 ==================== */

static void signal_obj_destroy(void *args)
{
    struct shared_signal *signal = (struct shared_signal *)args;

    /*
     * shared_signal 是共享 pending queue 和 POSIX timer 树的最终宿主。
     * 当最后一个引用离开时，需要在这里把共享对象拥有的信号节点和 timer 都完整收口。
     */
    sigqueue_purge(&signal->sig_queue);

    ktimer_cancel(signal->itimer);

    free(signal->itimer);

    signal->itimer = NULL;

    posix_timer_clean_shared(signal);

    if (signal->posix_timer_root)
    {
        free(signal->posix_timer_root);
        signal->posix_timer_root = NULL;
    }

    free(args);
}

struct process_obj *signal_obj_alloc(pcb_t pcb)
{
    struct shared_signal *signal;

    signal = calloc(1, sizeof(struct shared_signal));

    spin_lock_init(&signal->siglock);

    process_sigqueue_init(&signal->sig_queue);

    signal->itimer_interval = 0;

    atomic_set(&signal->next_posix_timer_id, 0);

    signal->posix_timer_root = calloc(1, sizeof(*signal->posix_timer_root));
    (*signal->posix_timer_root) = RB_ROOT_INIT;

    signal->itimer = calloc(1, sizeof(struct ktimer));

    return process_obj_create(pcb, signal, signal_obj_destroy);
}

int signal_obj_ref(pcb_t parent, pcb_t child)
{
    child->signal = process_obj_ref(child, parent->signal);
    return 0;
}

/* ==================== fork/copy ==================== */

int copy_sighand(unsigned long clone_flags, pcb_t child)
{
    irq_flags_t irq_flag = 0;
    irq_flags_t cur_irq_flag = 0;
    struct shared_sighand *sighand;
    pcb_t cur_pcb = ttosProcessSelf();
    struct shared_sighand *cur_sighand;
    struct shared_sighand tmp_sighand;

    if (cur_pcb)
    {
        cur_sighand = sighand_shared_get(cur_pcb);
    }
    else
    {
        memset(&tmp_sighand, 0, sizeof(struct shared_sighand));
        cur_sighand = &tmp_sighand;
    }

    if (clone_flags & CLONE_SIGHAND)
    {
        if (cur_pcb)
        {
            sighand_obj_ref(cur_pcb, child);
        }
        else
        {
            KLOG_E("fail at %s:%d", __FILE__, __LINE__);
            return -1;
        }

        return 0;
    }

    child->sighand = sighand_obj_alloc(child);
    if (!child->sighand)
    {
        return -ENOMEM;
    }

    sighand = sighand_shared_get(child);

    if (cur_pcb)
    {
        sighand_lock(cur_pcb, &cur_irq_flag);
    }

    sighand_lock(child, &irq_flag);
    memcpy(sighand->sig_action, cur_sighand->sig_action, _NSIG * sizeof(struct k_sigaction));
    sighand_unlock(child, &irq_flag);

    if (cur_pcb)
    {
        sighand_unlock(cur_pcb, &cur_irq_flag);
    }

    return 0;
}

int copy_signal(unsigned long clone_flags, pcb_t child)
{
    struct process_obj *sig;
    struct shared_signal *new_sig;

    if (clone_flags & CLONE_SIGHAND)
    {
        signal_obj_ref(ttosProcessSelf(), child);
        return 0;
    }

    sig = signal_obj_alloc(child);
    child->signal = sig;
    if (!sig)
        return -ENOMEM;

    new_sig = PROCESS_OBJ_GET(sig, struct shared_signal *);

    atomic_set(&new_sig->next_posix_timer_id, 0);
    (*new_sig->posix_timer_root) = RB_ROOT_INIT;

    ktimer_init(new_sig->itimer, itimer_handler, CLOCK_MONOTONIC, KTIMER_MODE_REL);
    itimer_pid_set(new_sig->itimer, get_process_pid(child));

    return 0;
}

int fork_signal(unsigned long clone_flags, pcb_t child)
{
    int ret;

    memset(&child->saved_sigmask, 0, sizeof(k_sigset_t));
    process_sigqueue_init(&child->sig_queue);

    ret = copy_sighand(clone_flags, child);
    if (ret)
    {
        return ret;
    }

    /* sighand 先决定 handler 共享关系，signal 再决定 pending/timer 共享关系。 */
    return copy_signal(clone_flags, child);
}

/* ==================== signal_reset ==================== */

/*
 * 删除 pending 队列中的部分节点后，重新按剩余节点回填 sigset_pending，
 * 保证位图和链表内容保持一致。
 */
static void sigqueue_rebuild_pending_set(struct k_sigpending_queue *sigqueue)
{
    ksiginfo_entry_t *entry;

    signal_set_clear(&sigqueue->sigset_pending);

    list_for_each_entry(entry, &sigqueue->siginfo_list, node)
    {
        signal_set_add(&sigqueue->sigset_pending, entry->ksiginfo.si_signo);
    }
}

/*
 * exec 后 POSIX timer 本身会被销毁，因此由 timer 产生的 SI_TIMER pending
 * 也必须一并丢弃；普通 signal pending 则保留。
 */
static void sigqueue_flush_posix_timer_entries(struct k_sigpending_queue *sigqueue)
{
    ksiginfo_entry_t *cur;
    ksiginfo_entry_t *next;

    list_for_each_entry_safe(cur, next, &sigqueue->siginfo_list, node)
    {
        if (cur->ksiginfo.si_code != SI_TIMER)
        {
            continue;
        }

        list_del(&cur->node);
        if (cur->priv)
        {
            posix_timer_signal_drop(cur->priv);
        }
        siginfo_delete(cur);
    }

    sigqueue_rebuild_pending_set(sigqueue);
}

/*
 * 多线程/CLONE_SIGHAND 场景下，exec 不能继续修改共享 sighand。
 * 这里先复制一份当前 disposition，后续 reset 只作用于当前进程。
 */
static int signal_exec_unshare_sighand(pcb_t pcb)
{
    irq_flags_t irq_flag = 0;
    struct process_obj *old_obj;
    struct process_obj *new_obj;
    struct shared_sighand *old_sighand;
    struct shared_sighand *new_sighand;

    if (!pcb || !pcb->sighand || pcb->sighand->really->ref == 1)
    {
        return 0;
    }

    old_obj = pcb->sighand;
    old_sighand = sighand_shared_get(pcb);

    new_obj = sighand_obj_alloc(pcb);
    if (!new_obj)
    {
        return -ENOMEM;
    }

    new_sighand = PROCESS_OBJ_GET(new_obj, struct shared_sighand *);

    /* 在锁保护下复制当前 disposition，避免和并发 sigaction 修改打架。 */
    sighand_lock(pcb, &irq_flag);
    memcpy(new_sighand->sig_action, old_sighand->sig_action, sizeof(new_sighand->sig_action));
    sighand_unlock(pcb, &irq_flag);

    /* 当前进程切到新的私有 sighand，旧共享对象按引用计数释放。 */
    pcb->sighand = new_obj;
    process_obj_destroy(old_obj);

    return 0;
}

int signal_reset(pcb_t pcb)
{
    irq_flags_t hand_irq_flag = 0;
    struct shared_signal *signal = signal_shared_get(pcb);
    struct shared_sighand *sighand = sighand_shared_get(pcb);
    struct k_sigaction *action;
    sighandler_t handler;
    int ret;
    int i;

    ret = signal_exec_unshare_sighand(pcb);
    if (ret)
    {
        return ret;
    }

    /*
     * exec 后 POSIX timer 会被删除，但普通 pending signal、signal mask 和
     * 传统 ITIMER_REAL/alarm 语义要保留。
     */
    /* 先收掉 POSIX timer 树本身，避免旧映像里的 timer 继续向新映像送信号。 */
    posix_timer_clean();

    sighand = sighand_shared_get(pcb);
    signal = signal_shared_get(pcb);

    sighand_lock(pcb, &hand_irq_flag);

    /*
     * exec 成功后不再恢复旧信号处理现场，但当前生效的 blocked mask 需要保留。
     */
    pcb->need_restore_blocked = 0;
    /* 仅清理“临时保存的掩码恢复现场”，不改当前 blocked。 */
    memset(&pcb->real_blocked, 0, sizeof(pcb->real_blocked));
    memset(&pcb->saved_sigmask, 0, sizeof(pcb->saved_sigmask));

    /*
     * 只丢弃因 POSIX timer 挂入的 SI_TIMER pending；普通 pending signal
     * 需要跨 exec 保留。
     */
    sigqueue_flush_posix_timer_entries(sigqueue_private_get(pcb));
    sigqueue_flush_posix_timer_entries(&signal->sig_queue);

    /*
     * POSIX 语义：
     *   - 仅把 caught handler 重置为 SIG_DFL
     *   - SIG_IGN / SIG_DFL disposition 保留
     *   - SA_ONSTACK 在 exec 后失效
     */
    for (i = 0; i < _NSIG; i++)
    {
        action = &sighand->sig_action[i];
        handler = sigaction_handler_get(action);

        if (handler != SIG_DFL && handler != SIG_IGN)
        {
            /* 用户自定义 handler 不能跨 exec 继承，统一回落到默认处理。 */
            memset(action, 0, sizeof(*action));
            action->__sa_handler._sa_handler = SIG_DFL;
            continue;
        }

        /* 即使 disposition 保留，exec 后备用信号栈绑定关系也要失效。 */
        action->sa_flags &= ~SA_ONSTACK;
    }

    sighand_unlock(pcb, &hand_irq_flag);

    /* 用户态 altstack 需要和 SA_ONSTACK 一起失效，并重算当前 pending 状态。 */
    altstack_reset(pcb);
    signal_pending_recalc_current();

    return 0;
}
