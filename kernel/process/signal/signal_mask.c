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
#include <signal.h>
#include <ttos.h>
#include <ttosBase.h>
#include <ttosProcess.h>
#include <signal_internal.h>
#include <tglock.h>

#undef KLOG_TAG
#define KLOG_TAG "signal"
#include <klog.h>

void signal_cpu_notify(unsigned int cpu_index);

/* ==================== 辅助函数 ==================== */

static bool thread_group_has_single_thread(pcb_t pcb)
{
    struct list_head *head = &pcb->group_leader->thread_group;

    /* 线程组链表始终包含 leader 本身，next == prev 说明只有一个成员 */
    return head->next == head->prev;
}

static void signal_redirect_shared_pending(pcb_t pcb, k_sigset_t *newblocked)
{
    k_sigset_t retarget;
    k_sigset_t can_handle;
    irq_flags_t sighand_flag;
    irq_flags_t tg_flag;
    pcb_t t;

    /* 持 sighand_lock 安全读取共享 pending 集合 */
    sighand_lock(pcb, &sighand_flag);
    signal_set_and(&retarget, sigqueue_shared_pending_get(pcb), newblocked);
    sighand_unlock(pcb, &sighand_flag);

    if (signal_set_is_empty(&retarget))
    {
        return;
    }

    /*
     * 共享 pending 仍然挂在线程组级队列里；这里做的只是“谁该尽快重新检查它”。
     * 因此要给真正能接手的线程打 SIGPENDING 标记，而不是留在当前线程身上。
     */
    tg_lock(pcb->group_leader, &tg_flag);

    for_each_thread_in_tgroup(pcb, t)
    {
        /* 跳过当前线程自身，只考虑其他线程 */
        if (t == pcb)
        {
            continue;
        }

        /* can_handle = retarget & ~t->blocked：t 未屏蔽、能接收的信号子集 */
        signal_set_and_not(&can_handle, &retarget, &t->blocked);
        if (!signal_set_is_empty(&can_handle))
        {
            /* 标记目标线程有共享 pending 可处理，并发 IPI 触发其重新检查。 */
            pcb_signal_pending_set(t);
            signal_cpu_notify(t->taskControlId->smpInfo.cpuIndex);

            /* 从待转集合中移除已分配给 t 的信号 */
            signal_set_and_not(&retarget, &retarget, &can_handle);
            /* 所有信号均已找到接收方，提前退出 */
            if (signal_set_is_empty(&retarget))
            {
                break;
            }
        }
    }

    tg_unlock(pcb->group_leader, &tg_flag);
}

/* ==================== 信号屏蔽码操作 ==================== */

void signal_mask_apply_current(k_sigset_t *newset)
{
    k_sigset_t newblocked;
    pcb_t pcb = ttosProcessSelf();

    /* 新旧掩码相同，无需任何操作 */
    if (signal_set_equal(&pcb->blocked, newset))
    {
        return;
    }

    /* SIGKILL/SIGSTOP 不可被屏蔽，强制从掩码中移除 */
    signal_set_del_immutable(newset);

    /*
     * 如果当前线程有 pending 信号，且线程组中存在其他线程：
     * 计算新增的屏蔽信号集（newset 中有、当前 blocked 中没有的部分），
     * 将共享 pending 队列中属于这些信号的项重新分配给其他能处理的线程，
     * 避免这些信号因当前线程屏蔽而无人处理。
     */
    if (pcb_signal_pending(pcb) && !thread_group_has_single_thread(pcb))
    {
        /* newblocked = newset & ~blocked：新增屏蔽的信号集(以前没屏蔽、现在变成屏蔽了) */
        signal_set_and_not(&newblocked, newset, &pcb->blocked);
        /* 将共享队列中属于 newblocked 的 pending 信号转交给其他线程处理 */
        signal_redirect_shared_pending(pcb, &newblocked);
    }

    pcb->blocked = *newset;
}

k_sigset_t *signal_mask_save_target(void)
{
    pcb_t pcb = ttosProcessSelf();
    k_sigset_t *res = &pcb->blocked;

    if (pcb->need_restore_blocked)
    {
        res = &pcb->saved_sigmask;
    }

    return res;
}

void signal_mask_restore_saved(void)
{
    pcb_t pcb = ttosProcessSelf();

    /* 没有信号需要处理，检查是否有信号屏蔽集需要恢复 */
    if (pcb && pcb->need_restore_blocked)
    {
        pcb->blocked = pcb->saved_sigmask;
        pcb->need_restore_blocked = false;
        memset(&pcb->saved_sigmask, 0, sizeof(k_sigset_t));
    }
}

static void sigmask_block(k_sigset_t *set, k_sigset_t *blocked, k_sigset_t *newset)
{
	signal_set_or(set, blocked, newset);
}

static void sigmask_unblock(k_sigset_t *set, k_sigset_t *blocked, k_sigset_t *newset)
{
	signal_set_and_not(set, blocked, newset);
}

static void sigmask_setmask(k_sigset_t *set, k_sigset_t *blocked, k_sigset_t *newset)
{
	*set = *newset;
}

typedef void (*sigmask_handler_t)(k_sigset_t *set, k_sigset_t *blocked, k_sigset_t *newset);

static const sigmask_handler_t sigmask_handlers[] = {
	[SIG_BLOCK]   = sigmask_block,
	[SIG_UNBLOCK] = sigmask_unblock,
	[SIG_SETMASK] = sigmask_setmask,
};

int kernel_sigprocmask(int how, k_sigset_t *newset, k_sigset_t *oldset)
{
	pcb_t pcb = ttosProcessSelf();
	k_sigset_t set;

	if (oldset)
    {
		*(k_sigset_t *)oldset = pcb->blocked;
    }

	if (how < 0 || how >= (int)(sizeof(sigmask_handlers) / sizeof(sigmask_handlers[0])))
	{
		return -EINVAL;
	}

	sigmask_handlers[how](&set, &pcb->blocked, newset);

	signal_mask_apply_current(&set);

	return 0;
}
