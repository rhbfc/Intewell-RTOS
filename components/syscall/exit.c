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

#include "futex.h"
#include "list.h"
#include "syscall_internal.h"
#include <assert.h>
#include <period_sched_group.h>
#include <process_signal.h>
#include <signal_internal.h>
#include <sys/wait.h>
#include <tasklist_lock.h>
#include <time/posix_timer.h>
#include <ttos.h>
#include <ttosInterTask.inl>
#include <ttosProcess.h>
#include <uaccess.h>
#include <trace.h>

#undef KLOG_TAG
#define KLOG_TAG "Exit"
#include <klog.h>

#undef KLOG_D
#define KLOG_D(...)

void __weak arch_switch_to_kernel_mmu(void){};

/**
 * @brief 系统调用实现：终止进程。
 *
 * 该函数实现了一个系统调用，用于终止当前进程。
 * 进程的所有资源将被释放，状态码返回给父进程。
 *
 * @param[in] error_code 退出状态码：
 *                 - 0: 正常退出
 *                 - 非0: 错误退出
 * @return 从不返回。
 *
 * @note 1. 该调用永远不会返回到调用进程。
 *       2. 退出状态可通过wait/waitpid获取。
 *       3. 进程的所有资源都会被释放。
 *       4. 子进程会被init进程接管。
 */
DEFINE_SYSCALL(exit, (int error_code))
{
    pcb_t pcb = ttosProcessSelf();
    assert(pcb != NULL);

    if (pcb == pcb->group_leader && list_count(&pcb->thread_group) > 1)
    {
        /* 如果是线程组的组长，且线程组不为空，则退出线程组 */
        process_exit_group(error_code, true);
    }

    if (pcb->clear_child_tid)
    {
        uint32_t __user *clear_child_tid = (uint32_t __user *)pcb->clear_child_tid;

        pcb->clear_child_tid = 0;
        int value = 0;

        copy_to_user(clear_child_tid, &value, sizeof(int));
        SYSCALL_FUNC(futex)(clear_child_tid, FUTEX_WAKE, 1, NULL, NULL, 0);
    }

    pcb->exit_code = error_code;

    process_exit(pcb);

    return 0;
}

/**
 * @brief 进程退出函数。
 *
 * 该函数负责进程的退出流程，包括资源释放、状态更新等。
 *
 * @param pcb 进程控制块。
 */
void process_exit(pcb_t pcb)
{
    KLOG_D("process_exit called pcb %p(%d)", pcb, pcb->taskControlId->tid);

    if (pcb->vfork_done)
    {
        vfork_exit_wake(pcb);
    }

    trace_exit();
    process_destroy(pcb);

    KLOG_D("process_exit finish");
}

/**
 * @brief 进程自动收割函数。
 *
 * 该函数决定子进程是否需要进入僵尸态。
 *
 * @param pcb 进程控制块。
 * @return true: 自动收割，false: 不自动收割。
 */
static bool process_autoreap(pcb_t pcb)
{
    /* 决定子进程是否需要进入僵尸态 */
    struct k_sigaction *sigact;
    irq_flags_t hand_irq_flag;
    bool autoreap = false;

    if (pcb->parent == NULL)
    {
        return true;
    }

    sighand_lock(pcb->parent, &hand_irq_flag);
    sigact = sigaction_get_ptr(pcb->parent, SIGCHLD);
    if (sigaction_handler_get(sigact) == SIG_ACT_IGN ||
        (sigact->sa_flags & SA_NOCLDWAIT))
    {
        /**
         * 如果父进程对SIGCHLD设置了IGnore的处理行为，或者设置了SA_NOCLDWAIT 标志位，
         * 则自动收集僵尸态的子进程，不留给waitpid处理，进而无需成为僵尸进程。
         * 但是同样需要唤醒在等待的父进程。
         */
        autoreap = true;
    }

    sighand_unlock(pcb->parent, &hand_irq_flag);

    return autoreap;
}

/**
 * @brief 通知父进程函数。
 *
 * 该函数负责通知父进程，并唤醒父进程。
 *
 * @param pcb 进程控制块。
 */
static void do_notify_parent(pcb_t pcb)
{
    pid_t pid;

    if (pcb->parent == NULL)
    {
        return;
    }

    process_wakeup_waiter(pcb);
    pid = get_process_pid(pcb->parent);
    kernel_signal_send(pid, TO_PROCESS, SIGCHLD, SI_USER, 0);
}

/**
 * @brief 托孤函数。
 *
 * 该函数负责为当前进程的子进程寻找新的父进程。
 *
 * @param pcb 进程控制块。
 */
static void child_reparent(pcb_t pcb)
{
    if (pcb->first_child != NULL)
    {
        pcb_t init_pcb = pcb_get_by_pid(1);
        if (init_pcb != NULL)
        {
            pcb_t child = pcb->first_child;
            while (child != NULL)
            {
                child->parent = init_pcb;
                child = child->sibling;
            }

            // 将当前的子进程链表连接到 init_pcb 的子进程链表末尾
            if (init_pcb->first_child == NULL)
            {
                init_pcb->first_child = pcb->first_child;
            }
            else
            {
                pcb_t last_child = init_pcb->first_child;
                while (last_child->sibling != NULL)
                {
                    last_child = last_child->sibling;
                }
                last_child->sibling = pcb->first_child;
            }
        }
        else
        {
            pcb_t child = pcb->first_child;
            while (child != NULL)
            {
                child->parent = NULL;
                child = child->sibling;
            }
        }
    }
}

/**
 * @brief 进程销毁函数。
 *
 * 该函数负责进程的销毁流程，包括资源释放、状态更新等。
 *
 * @param pcb 进程控制块。
 */
void process_destroy(pcb_t pcb)
{
    bool autoreap = true;
    struct process_obj *obj, *n;
    TASK_ID task = pcb->taskControlId;
    T_TTOS_ReturnCode ret = TTOS_OK;

    arch_switch_to_kernel_mmu();

    /* 等待PCB对象可被释放，避免在删除任务时，任务还在使用PCB对象 */
    pcb_wait_to_free(pcb);

    arch_cpu_int_disable();

    tasklist_lock();

    signal_private_pending_flush(pcb);

    /* 避免进程退出和进程组退出导致重复清理 */
    if (pcb->exit_state == EXIT_ZOMBIE)
    {
        arch_cpu_int_enable();
        tasklist_unlock();
        return;
    }

    KLOG_D("process destroy, tid %d, pcb %p %s", pcb->taskControlId->tid, pcb, pcb->cmd_name);

    period_sched_group_exit(pcb);


    /* 托孤 */
    child_reparent(pcb);

    /* 删除除了pid以外的所有进程对象 */
    list_for_each_entry_safe(obj, n, &pcb->obj_list, list)
    {
        if (obj == pcb->pid)
        {
            continue;
        }
        process_obj_destroy(obj);
    }

    /* 根据状态决定是否自动执行收割，不进入僵尸态 */
    autoreap = process_autoreap(pcb);
    if (autoreap)
    {
        /* 进程不进入僵尸态，稍后直接释放所有资源 */
        pcb->exit_state = EXIT_DEAD;
        pcb->taskControlId->state |= TTOS_TASK_EXIT_DEAD;
        KLOG_D("pcb %p(%d) is DEAD", pcb, pcb->taskControlId->tid);
    }
    else
    {
        /* 设置为僵尸态，等待waitpid调用并收割 */
        pcb->exit_state = EXIT_ZOMBIE;
        pcb->taskControlId->state |= TTOS_TASK_EXIT_ZOMBIE;
        KLOG_D("pcb %p(%d) is ZOMBIE", pcb, pcb->taskControlId->tid);
    }

    /* 退出进程并释放所有资源 */
    if (pcb->exit_state == EXIT_DEAD)
    {
        process_release_zombie(pcb);
        arch_cpu_int_enable();
        /* 解锁 */
        tasklist_unlock();
        goto delete_exit;
    }
    else
        /* 通知并唤醒父进程，由父进程释放自身的其他资源 */
        do_notify_parent(pcb);

    arch_cpu_int_enable();

    /* 解锁 */
    tasklist_unlock();

    /* 对于线程，在此处认为已经退出，保留pcb信息，设置标志位并挂起，等待回收僵尸态时唤醒 */
    if (task->tid >= CONFIG_PROCESS_MAX)
    {
        ret = task_delete_suspend(task);
        if (ret != TTOS_OK)
        {
            printk("%s %d, thread exit and suspend failed, ret = %d\n", __func__, __LINE__, ret);
            assert(0);
        }
    }

delete_exit:
    /* task与pcb分离 */
    task->ppcb = NULL;

    KLOG_D("task delete called");
    TTOS_DeleteTask(task);
    assert(0);
}
