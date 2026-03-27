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

#ifndef __SIGNAL_INTERNAL_H__
#define __SIGNAL_INTERNAL_H__

#include <process_signal.h>

/*
 * signal_internal.h 只放 signal 子系统内部协作接口：
 *   - 队列、锁、状态查询、发送前预处理
 *   - 供 wait4/exit/time/arch 等内核内部模块复用的内部 helper
 * 不作为对外稳定接口使用。
 */

#define SIGNAL_CTRL_GROUP_STOP_PENDING      (1UL << 0)
#define SIGNAL_CTRL_CONTINUE_REPORT_PENDING (1UL << 1)
#define SIGNAL_CTRL_STOP_SIG_SHIFT          8
#define SIGNAL_CTRL_STOP_SIG_MASK           (0xffUL << SIGNAL_CTRL_STOP_SIG_SHIFT)

static inline int signal_ctrl_stop_signo(unsigned long jobctl)
{
    return (int)((jobctl & SIGNAL_CTRL_STOP_SIG_MASK) >> SIGNAL_CTRL_STOP_SIG_SHIFT);
}

static inline unsigned long signal_ctrl_set_stop_signo(unsigned long jobctl, int signo)
{
    jobctl &= ~SIGNAL_CTRL_STOP_SIG_MASK;
    jobctl |= ((unsigned long)signo << SIGNAL_CTRL_STOP_SIG_SHIFT) & SIGNAL_CTRL_STOP_SIG_MASK;
    return jobctl;
}

/* Shared signal/sighand accessors and locks */
struct shared_sighand *sighand_shared_get(pcb_t pcb);
struct shared_signal *signal_shared_get(pcb_t pcb);
struct k_sigaction *sigaction_get_ptr(pcb_t pcb, int signo);
sighandler_t sigaction_handler_get(const struct k_sigaction *action);
sighandler_t sighandler_get(pcb_t pcb, int signo);
void signal_lock(pcb_t pcb, irq_flags_t *irq_flag);
void signal_unlock(pcb_t pcb, irq_flags_t *irq_flag);
void sighand_lock(pcb_t pcb, irq_flags_t *irq_flag);
void sighand_unlock(pcb_t pcb, irq_flags_t *irq_flag);

/* Signal object helpers used by fork/exec/time paths */
int sighand_obj_ref(pcb_t parent, pcb_t child);
int signal_obj_ref(pcb_t parent, pcb_t child);
int copy_sighand(unsigned long clone_flags, pcb_t child);
int copy_signal(unsigned long clone_flags, pcb_t child);

/* Queue accessors and operations */
struct k_sigpending_queue *sigqueue_private_get(pcb_t pcb);
struct k_sigpending_queue *sigqueue_shared_get(pcb_t pcb);
k_sigset_t *sigqueue_private_pending_get(pcb_t pcb);
k_sigset_t *sigqueue_shared_pending_get(pcb_t pcb);
int sigqueue_isempty(struct k_sigpending_queue *sigqueue);
int sigqueue_ismember(struct k_sigpending_queue *sigqueue, int signo);
ksiginfo_entry_t *siginfo_create(int signo, int code, ksiginfo_t *info);
void siginfo_delete(ksiginfo_entry_t *siginfo);
void sigqueue_enqueue(struct k_sigpending_queue *sigqueue, ksiginfo_entry_t *siginfo);
ksiginfo_entry_t *sigqueue_dequeue_single(struct k_sigpending_queue * sigqueue, int signo);
void sigqueue_discard_sigset(struct k_sigpending_queue *sigqueue, k_sigset_t *sigset);
void sigqueue_discard(struct k_sigpending_queue *sigqueue, int signo);
void sigqueue_remove(pcb_t pcb, k_sigset_t *remove_sigset, struct k_sigpending_queue *sigqueue);
void sigqueue_remove_shared(pcb_t pcb, k_sigset_t *remove_sigset);
void sigqueue_remove_private(pcb_t pcb, k_sigset_t *remove_sigset);
void sigqueue_purge(struct k_sigpending_queue *sigqueue);
void signal_private_pending_flush(pcb_t pcb);
void process_sigqueue_init(struct k_sigpending_queue *sigq);
int sigqueue_examine(struct k_sigpending_queue *sigqueue, k_sigset_t *pending);

/* Signal send/notify helpers */
int kernel_signal_send_with_worker(pid_t pid, int to, long signo, long code, ksiginfo_t *kinfo);
void signal_cpu_notify(unsigned int cpu_index);
bool sig_immutable(int signo);
bool signal_num_valid(int sig);
bool signal_prepare_send(int sig, pcb_t pcb, bool force);
bool signal_should_queue(int sig, pcb_t pcb);
void signal_notify_parent_and_leader(pcb_t child_process, int signo, bool is_stop);
void kernel_signal_pending(pcb_t pcb, k_sigset_t *pending);
bool signal_should_report_status(pcb_t pcb, int signo);
void signal_pending_recalc_current(void);
void signal_pending_recalc_current_locked(void);

/* Signal query helpers */
bool signal_default_ignored(int signo);
bool signal_send_ignored(pcb_t pcb, int signo);
bool is_rt_signo(int signo);
bool is_stop_set(int signo);
bool is_thread_group_leader(pcb_t p);
int signal_find_next_pending(k_sigset_t *pending, k_sigset_t *mask);
int sigqueue_peek(struct k_sigpending_queue *sigqueue, k_sigset_t *mask);
int signal_dequeue(pcb_t pcb, k_sigset_t *mask, siginfo_t *uinfo);

/* Alternative stack helpers used by arch signal code */
int altstack_flags(unsigned long sp);
int altstack_sp_in_range(unsigned long sp);
int altstack_on_stack(unsigned long sp);
void altstack_reset(pcb_t pcb);

#endif /* __SIGNAL_INTERNAL_H__ */
