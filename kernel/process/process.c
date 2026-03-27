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
#include <cache.h>
#include <errno.h>
#include <spinlock.h>
#include <stdio.h>
#include <ttos.h>
#include <ttosInterTask.inl>
#include <ttosMM.h>
#include <ttosProcess.h>

#include <process_exit.h>
#include <tasklist_lock.h>
#include <tglock.h>

#undef KLOG_TAG
#define KLOG_TAG "Process"
#include <klog.h>

#undef KLOG_D
#define KLOG_D(...)

extern void kernel_set_tls(uintptr_t tls);
extern __noreturn void arch_run2user(void);

static void remove_from_group(pcb_t pcb)
{
    irq_flags_t irq_flag;
    /* 从线程组退出 */
    tg_lock(pcb->group_leader, &irq_flag);
    list_del(&pcb->sibling_node);
    tg_unlock(pcb->group_leader, &irq_flag);

    /* 从进程组中移除该进程 */
    if (pcb->group_leader == pcb)
    {
        pgroup_t group = process_pgrp_find(process_pgid_get_byprocess(pcb));
        if (group)
        {
            pgrp_remove(group, pcb);
            pgrp_put(group);
        }
    }
}

void pcb_lock(pcb_t pcb, long *flags)
{
    unsigned long irq_flag;

    ttos_int_lock(irq_flag);
    *flags = irq_flag;
}

void pcb_unlock(pcb_t pcb, long *flags)
{
    unsigned long irq_flag = *flags;

    ttos_int_unlock(irq_flag);
}

void foreach_process_child(pcb_t pcb, void (*func)(pcb_t, void *), void *param)
{
    pcb_t child;
    long flags;

    pcb_lock(pcb, &flags);
    pcb_t node = pcb->parent->first_child;
    while (node->sibling != NULL)
    {
        func(node, param);
        node = node->sibling;
    }
    pcb_unlock(pcb, &flags);
}

/* 移除父进程、兄弟进程的关系 */
static void remove_from_family(pcb_t pcb)
{
    if (pcb->parent)
    {
        if (pcb->parent->first_child == pcb)
        {
            pcb->parent->first_child = pcb->sibling;
        }
        else
        {
            pcb_t node = pcb->parent->first_child;
            while (node->sibling != NULL)
            {
                if (node->sibling == pcb)
                {
                    node->sibling = pcb->sibling;
                    break;
                }

                node = node->sibling;
            }
        }
        pcb->sibling = NULL; // 确保移除后 sibling 指针被清空
        pcb->parent = NULL;
    }
    else
    {
        /* do nothing */
    }
}

/* 收割僵尸进程，释放进程的所有资源 */
int process_release_zombie(pcb_t pcb)
{
    irq_flags_t flags;
    TASK_ID task = pcb->taskControlId;
    T_TTOS_ReturnCode ret = TTOS_OK;

    if (!pcb)
    {
        return 0;
    }

    KLOG_D("process_release_zombie called, pcb %p", pcb);

    tasklist_lock();

    /* 唤醒后继续执行任务删除的流程 */
    if (task->state & TTOS_TASK_WAITING_DELETE)
    {
        ret = task_delete_resume(task);
        if (ret != TTOS_OK)
        {
            printk("%s %d, resume task failed, ret = %d\n", __func__, __LINE__, ret);
            assert(0);
        }
    }

    /* 移除父进程、兄弟进程的关系 */
    remove_from_family(pcb);

    /* 从进程组、线程组移除进程 */
    remove_from_group(pcb);

    process_obj_destroy(pcb->pid);

    tg_lock_delete(pcb);

    free(pcb);

    tasklist_unlock();

    KLOG_D("process_release_zombie finish");

    return 0;
}

void process_wakeup_waiter(pcb_t pcb)
{
    struct waitpid_info *wi;
    pcb_t wait_parent;

    wait_parent = pcb->parent ? pcb->parent : pcb->group_leader->parent;
    if (!wait_parent)
    {
        return;
    }

    wi = get_process_waitinfo(wait_parent);
    wi->waker = pcb;

    KLOG_D("waitpid kick, child %p(%d)---> %s exit, notifty parent pcb %p---> %s", pcb,
           get_process_pid(pcb), pcb->cmd_name, wait_parent, wait_parent->cmd_name);
    /* @KEEP_COMMENT: 禁止调度器 */
    ttosDisableTaskDispatchWithLock();

    while (!list_is_empty(&(wi->waitQueue.queues.fifoQueue)))
    {
        /* @KEEP_COMMENT: 唤醒等待队列中的任务 */
        ttosDequeueTaskq(&(wi->waitQueue));
    }

    /* @KEEP_COMMENT: 重新使能调度 */
    ttosEnableTaskDispatchWithLock();
}

pcb_t ttosProcessSelf(void)
{
    return ttosGetRunningTask()->ppcb;
}

/**
 *  等待任意子进程或线程唤醒自身，对应 process_wakeup_waiter 接口
 *  这种阻塞、唤醒方式只适用于存在父子关系的进程或线程，不适用于进程组的其他进程退出唤醒阻塞进程。
 */
int wait_process(pcb_t target, wait_handle_t *wait_handle)
{
    pcb_t pcb = ttosProcessSelf();
    pcb_t zombie_found;

    if (pcb == NULL)
    {
        return -ECHILD;
    }

    tasklist_lock();

    /**
     * 进入等待前，先获取一次目标进程/线程的退出信息
     */
    if (wait_handle->options & __WALL)
    {
        zombie_found = wait_thread_zombie(target, wait_handle);
    }
    else
    {
        zombie_found = wait_process_zombie(target, wait_handle);
    }

    if (zombie_found)
    {
        /**
         * 将已经退出且符合条件的进程保存在父进程的waker属性中。
         * 如果pid > 0，根据target收割即可，无需保存；
         * 否则，需要在外层完成对处于僵尸状态的进程waker的收割
         */
        KLOG_D("before wait, we found process %p(pid:%d) already zombie", zombie_found,
               zombie_found->taskControlId->tid);

        get_process_waitinfo(pcb)->waker = zombie_found;
        tasklist_unlock();
        return 0;
    }

    /* 如果设置了WNOHANG选项，则不进入等待 */
    if (wait_handle->options & WNOHANG)
    {
        tasklist_unlock();
        return -EAGAIN;
    }

    KLOG_D("enqueue %p[%d]---> %s , waiting...", pcb, get_process_pid(pcb), pcb->cmd_name);

    /* @KEEP_COMMENT: 禁止调度器 */
    ttosDisableTaskDispatchWithLock();

    ttosGetRunningTask()->wait.returnCode = TTOS_OK;

    ttosEnqueueTaskq(&get_process_waitinfo(pcb)->waitQueue, TTOS_WAIT_FOREVER, TRUE);

    tasklist_unlock();

    /* @KEEP_COMMENT: 重新使能调度 */
    ttosEnableTaskDispatchWithLock();

    KLOG_D("process %p wake up from waitpid", pcb);

    /* @REPLACE_BRACKET: TTOS_OK */
    return -ttos_ret_to_errno(ttosGetRunningTask()->wait.returnCode);
}

#ifdef CONFIG_SHELL
#include <shell.h>

void showprocess(pcb_t pcb, void *arg)
{
    printf("%7d pts/0    00:00:00 %s\n", get_process_pid(pcb), pcb->cmd_name);
}

int process_status_cmd(int argc, const char *argv[])
{
    printf("    PID TTY          TIME CMD\n");
    process_foreach(showprocess, NULL);
    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) |
                     SHELL_CMD_DISABLE_RETURN,
                 ps, process_status_cmd, process status);

#endif /* CONFIG_SHELL */
