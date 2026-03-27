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
#include <atomic.h>
#include <errno.h>
#include <stdint.h>
#include <list.h>
#include <tglock.h>
#include <ttosBase.h>
#include <ttosProcess.h>
#include <ttos_init.h>

#undef KLOG_TAG
#define KLOG_TAG "pgroup"
#include <klog.h>

#undef KLOG_D
#define KLOG_D(...)

static struct list_head process_group_list;
static ttos_spinlock_t pgroup_list_lock;

int pgrp_init(void)
{
    INIT_LIST_HEAD(&process_group_list);
    spin_lock_init(&pgroup_list_lock);

    return 0;
}

static void pgrp_destroy(pgroup_t pgrp)
{
    if (pgrp->lock)
    {
        TTOS_DeleteMutex(pgrp->lock);
    }

    free(pgrp);
}

static void pgrp_inc_ref(pgroup_t pgrp)
{
    atomic_inc(&pgrp->ref);
}

void pgrp_put(pgroup_t pgrp)
{
    if (pgrp == NULL)
    {
        return;
    }

    if (atomic_dec_return(&pgrp->ref) == 0)
    {
        pgrp_destroy(pgrp);
    }
}

static void pgrp_publish(pgroup_t group)
{
    long flag = 0;

    pgrp_inc_ref(group);

    spin_lock_irqsave(&pgroup_list_lock, flag);
    list_add(&group->global_pgroup_node, &process_group_list);
    spin_unlock_irqrestore(&pgroup_list_lock, flag);
}

static void pgrp_unpublish(pgroup_t group)
{
    long flag = 0;
    bool is_published = false;

    spin_lock_irqsave(&pgroup_list_lock, flag);
    if (group->global_pgroup_node.next != NULL)
    {
        list_del(&group->global_pgroup_node);
        is_published = true;
    }
    spin_unlock_irqrestore(&pgroup_list_lock, flag);

    if (is_published)
    {
        pgrp_put(group);
    }
}

static bool pgrp_is_active_locked(pgroup_t group)
{
    return group->leader != NULL;
}

static void pgrp_mark_empty_locked(pgroup_t group)
{
    group->leader = NULL;
}

static void pgrp_lock_pair(pgroup_t group1, pgroup_t group2)
{
    uintptr_t addr1 = (uintptr_t)group1;
    uintptr_t addr2 = (uintptr_t)group2;

    if (group1 == group2)
    {
        TTOS_ObtainMutex(group1->lock, TTOS_WAIT_FOREVER);
        return;
    }

    if (addr1 < addr2)
    {
        TTOS_ObtainMutex(group1->lock, TTOS_WAIT_FOREVER);
        TTOS_ObtainMutex(group2->lock, TTOS_WAIT_FOREVER);
        return;
    }

    TTOS_ObtainMutex(group2->lock, TTOS_WAIT_FOREVER);
    TTOS_ObtainMutex(group1->lock, TTOS_WAIT_FOREVER);
}

static void pgrp_unlock_pair(pgroup_t group1, pgroup_t group2)
{
    uintptr_t addr1 = (uintptr_t)group1;
    uintptr_t addr2 = (uintptr_t)group2;

    if (group1 == group2)
    {
        TTOS_ReleaseMutex(group1->lock);
        return;
    }

    if (addr1 < addr2)
    {
        TTOS_ReleaseMutex(group2->lock);
        TTOS_ReleaseMutex(group1->lock);
        return;
    }

    TTOS_ReleaseMutex(group1->lock);
    TTOS_ReleaseMutex(group2->lock);
}

pgroup_t pgrp_find_and_inc_ref(pid_t pgid)
{
    return process_pgrp_find(pgid);
}

pgroup_t process_pgrp_find(pid_t pgid)
{
    pcb_t pcb;
    long flag = 0;
    pgroup_t group = NULL;

    if (pgid < 0)
    {
        return NULL;
    }

    if (pgid == 0)
    {
        pcb = ttosProcessSelf();
        if (pcb)
        {
            pgid = get_process_pid(ttosProcessSelf());
        }
        else
        {
            return NULL;
        }
    }

    spin_lock_irqsave(&pgroup_list_lock, flag);

    list_for_each_entry(group, &process_group_list, global_pgroup_node)
    {
        if (group->pgid == pgid)
        {
            pgrp_inc_ref(group);
            spin_unlock_irqrestore(&pgroup_list_lock, flag);

            return group;
        }
    }

    spin_unlock_irqrestore(&pgroup_list_lock, flag);

    return NULL;
}

pgroup_t pgrp_create(pcb_t leader)
{
    pgroup_t group = NULL;

    if (leader == NULL)
    {
        return NULL;
    }

    group = calloc(1, sizeof(struct pgroup));
    if (group != NULL)
    {
        INIT_LIST_HEAD(&(group->process));

        if (TTOS_CreateMutex(1, 0, &(group->lock)) != TTOS_OK)
        {
            free(group);
            return NULL;
        }

        atomic_set(&group->ref, 1);

        group->leader = leader;
        group->sid = 0;
        group->is_orphaned = 0;
        group->pgid = get_process_pid(leader);

        /* 对象初始化完成后再加入全局进程组链表 */
        pgrp_publish(group);
    }

    return group;
}

static int pgrp_attach_member_locked(pgroup_t group, pcb_t process)
{
    if (!pgrp_is_active_locked(group))
    {
        return -ESRCH;
    }

    process->pgid = group->pgid;
    process->pgrp = group;
    process->sid = group->sid;
    list_add_after(&(process->pgrp_node), &(group->process));

    pgrp_inc_ref(group);

    return 0;
}

int pgrp_insert(pgroup_t group, pcb_t process)
{
    int ret;

    if (group == NULL || process == NULL)
    {
        return -EINVAL;
    }

    TTOS_ObtainMutex(group->lock, TTOS_WAIT_FOREVER);

    ret = pgrp_attach_member_locked(group, process);

    TTOS_ReleaseMutex(group->lock);

    return ret;
}

static int pgrp_remove_member_locked(pgroup_t group, pcb_t process, bool *is_empty)
{
    *is_empty = false;

    if (process->pgrp != group)
    {
        return -ESRCH;
    }

    list_del(&(process->pgrp_node));

    process->pgrp = NULL;
    process->pgid = 0;
    process->sid = 0;

    *is_empty = list_empty(&(group->process));
    if (*is_empty)
    {
        /* 组在持锁下先进入 empty 状态，随后再由 list 锁路径摘链。 */
        pgrp_mark_empty_locked(group);
    }

    return 0;
}

static int pgrp_transfer_member_locked(pgroup_t old_group, pgroup_t new_group, pcb_t process,
                                       bool *old_group_empty)
{
    *old_group_empty = false;

    if (process->pgrp != old_group)
    {
        return -ESRCH;
    }

    if (!pgrp_is_active_locked(new_group))
    {
        return -ESRCH;
    }

    list_del(&(process->pgrp_node));

    *old_group_empty = list_empty(&(old_group->process));
    if (*old_group_empty)
    {
        pgrp_mark_empty_locked(old_group);
    }

    list_add_after(&(process->pgrp_node), &(new_group->process));
    process->pgid = new_group->pgid;
    process->pgrp = new_group;
    process->sid = new_group->sid;

    pgrp_inc_ref(new_group);

    return 0;
}

static void pgrp_finish_old_group_move(pgroup_t old_group, bool old_group_empty)
{
    if (old_group_empty)
    {
        pgrp_unpublish(old_group);
    }

    /* 迁组成功后，旧组需要同时释放成员引用和本次查找获得的临时引用。 */
    pgrp_put(old_group);
    pgrp_put(old_group);
}

bool pgrp_remove(pgroup_t group, pcb_t process)
{
    bool is_empty = false;
    int ret;

    /* parameter check */
    if (group == NULL || process == NULL)
    {
        return false;
    }

    TTOS_ObtainMutex(group->lock, TTOS_WAIT_FOREVER);

    ret = pgrp_remove_member_locked(group, process, &is_empty);

    TTOS_ReleaseMutex(group->lock);

    if (ret != 0)
    {
        return false;
    }

    if (is_empty)
    {
        pgrp_unpublish(group);
    }

    pgrp_put(group);

    return is_empty;
}

pid_t pgid_get_bypgrp(pgroup_t group)
{
    return group ? group->pgid : 0;
}

int pgrp_move(pgroup_t group, pcb_t process)
{
    int ret = 0;
    pgroup_t old_group;

    if (group == NULL || process == NULL)
    {
        return -EINVAL;
    }

    if (pgid_get_bypgrp(group) == process_pgid_get_byprocess(process))
    {
        return 0;
    }

    while (1)
    {
        bool old_group_empty = false;
        bool moved = false;
        bool need_retry = false;

        old_group = process_pgrp_find(process_pgid_get_byprocess(process));
        if (old_group == NULL)
        {
            return -ESRCH;
        }

        if (old_group == group)
        {
            pgrp_put(old_group);
            return 0;
        }

        pgrp_lock_pair(old_group, group);

        if (process->pgrp == old_group)
        {
            ret = pgrp_transfer_member_locked(old_group, group, process, &old_group_empty);
            if (ret == 0)
            {
                moved = true;
            }
        }
        else
        {
            need_retry = true;
        }

        pgrp_unlock_pair(old_group, group);

        if (moved)
        {
            pgrp_finish_old_group_move(old_group, old_group_empty);
            return 0;
        }

        pgrp_put(old_group);

        if (ret != 0)
        {
            return ret;
        }

        if (need_retry)
        {
            continue;
        }
    }

    return ret;
}

void foreach_pgrp_process(foreach_pgrp_func func, pgroup_t group, void *param)
{
    pcb_t pcb;

    TTOS_ObtainMutex(group->lock, TTOS_WAIT_FOREVER);
    list_for_each_entry(pcb, &(group->process), pgrp_node)
    {
        func(pcb, param);
    }
    TTOS_ReleaseMutex(group->lock);
}

int pgrp_update_children_info(pgroup_t group, pid_t sid, pid_t pgid)
{
    struct list_head *node = NULL;
    pcb_t process = NULL;

    if (group == NULL)
    {
        return -EINVAL;
    }

    TTOS_ObtainMutex(group->lock, TTOS_WAIT_FOREVER);

    list_for_each(node, &(group->process))
    {
        process = (pcb_t)list_entry(node, struct T_TTOS_ProcessControlBlock, pgrp_node);

        if (sid != -1)
        {
            process->sid = sid;
        }
        if (pgid != -1)
        {
            process->pgid = pgid;
            process->pgrp = group;
        }
    }

    TTOS_ReleaseMutex(group->lock);
    return 0;
}

int setpgid(pid_t pid, pid_t pgid)
{
    pcb_t process, self_process;
    pgroup_t group = NULL;
    int err = 0;
    bool group_created = false;

    self_process = ttosProcessSelf();

    if (pid == 0)
    {
        pid = get_process_pid(self_process);
        process = self_process;
    }
    else
    {
        process = pcb_get_by_pid(pid);

        if (process == NULL)
        {
            return -ESRCH;
        }
    }

    if (pgid == 0)
    {
        pgid = pid;
    }
    if (pgid < 0)
    {
        return -EINVAL;
    }

    if (!pcb_get(process))
    {
        return -ESRCH;
    }

    if (process->group_leader->parent == self_process)
    {
        if (!pcb_flags_test(process, FORKNOEXEC))
        {
            err = -EACCES;
            goto exit;
        }
    }
    else
    {
        if (process != self_process)
        {
            err = -ESRCH;
            goto exit;
        }
    }

    if (pgid != pid)
    {
        group = process_pgrp_find(pgid);
        if (group == NULL)
        {
            group = pgrp_create(process);
            if (group == NULL)
            {
                err = -ENOMEM;
                goto exit;
            }
            group_created = true;
            err = pgrp_move(group, process);
        }
        else
        {
            err = pgrp_move(group, process);
        }
        if (err != 0 && group_created)
        {
            pgrp_unpublish(group);
        }
        pgrp_put(group);
    }
    else
    {
        group = process_pgrp_find(pgid);
        if (group == NULL)
        {
            group = pgrp_create(process);
            if (group == NULL)
            {
                err = -ENOMEM;
                goto exit;
            }
            group_created = true;
            err = pgrp_move(group, process);
        }
        else
        {
            if (process->pgid != group->pgid)
            {
                err = pgrp_move(group, process);
            }
        }
        if (err != 0 && group_created)
        {
            pgrp_unpublish(group);
        }
        pgrp_put(group);
    }

exit:
    pcb_put(process);
    return err;
}

void foreach_task_group(pcb_t pcb, void (*func)(pcb_t, void *), void *param)
{
    irq_flags_t flags;
    pcb_t thread;
    pcb_t next;

    tg_lock(pcb->group_leader, &flags);
    list_for_each_entry_safe(thread, next, &pcb->group_leader->thread_group, sibling_node)
    {
        func(thread, param);
    }
    tg_unlock(pcb->group_leader, &flags);
}

struct exit_ctx
{
    int count;
    pid_t thread[1024];
};

static void exit_task(pcb_t pcb, void *param)
{
    struct exit_ctx *ctx = param;
    /* 忽略已经或即将退出的进程 */
    if (pcb->exit_state)
    {
        return;
    }

    if (pcb == pcb->group_leader)
    {
        return;
    }
    if (pcb->taskControlId)
    {
        ctx->thread[ctx->count] = pcb->taskControlId->tid;
        ctx->count++;
        assert(ctx->count < 1024);
        /* 这里开锁 避免死锁 */
        // tg_unlock(pcb->group_leader);
        // kernel_signal_send(pcb->taskControlId->tid, TO_THREAD, SIGKILL, SI_KERNEL, NULL);
        // tg_lock(pcb->group_leader);

        // kernel_signal_send_with_worker(, TO_THREAD, SIGKILL, SI_KERNEL,
        //                                NULL);
    }
}

long process_exit_group(int exit_code, bool is_normal_exit)
{
    pcb_t self = ttosProcessSelf();
    assert(self != NULL);

    pcb_t leader = self->group_leader;
    assert(leader != NULL);

    struct exit_ctx ctx;

    if (self != leader)
    {
        leader->group_exit_status.group_request_terminate = true;
        leader->group_exit_status.exit_code = exit_code;
        leader->group_exit_status.is_normal_exit = is_normal_exit;

        assert(leader->taskControlId != NULL);

        kernel_signal_send(leader->taskControlId->tid, TO_THREAD, SIGKILL, SI_KERNEL, NULL);
        if (is_normal_exit)
        {
            self->exit_code = exit_code;
            process_exit(self);
        }
        else
        {
            self->exit_signal = exit_code;
            process_exit(self);
        }

        return 0;
    }

    if (leader->group_exit_status.is_terminated)
    {
        return 0;
    }

    leader->group_exit_status.exit_code = exit_code;
    leader->group_exit_status.is_terminated = true;
    leader->group_exit_status.is_normal_exit = is_normal_exit;

    ctx.count = 0;

    foreach_task_group(self, exit_task, &ctx);

    for (int i = 0; i < ctx.count; i++)
    {
        kernel_signal_send(ctx.thread[i], TO_THREAD, SIGKILL, SI_KERNEL, NULL);
    }

    // todo wait all thread exit
    while (1)
    {
        int thread_cnt;
        thread_cnt = list_count(&leader->thread_group);
        if (1 == thread_cnt)
        {
            break;
        }
        else
        {
            TTOS_SleepTask(2);
        }
    }

    if (is_normal_exit)
    {
        self->exit_code = exit_code;
        process_exit(self);
    }
    else
    {
        self->exit_signal = exit_code;
        process_exit(self);
    }
    return 0;
}

int sys_getpgid(pid_t pid)
{
    pcb_t process;

    process = pcb_get_by_pid(pid);

    if (process == NULL)
    {
        return -ESRCH;
    }

    return process_pgid_get_byprocess(process);
}

INIT_EXPORT_SYS(pgrp_init, "pgrp_init");
