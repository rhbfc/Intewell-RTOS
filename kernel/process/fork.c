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
#include <context.h>
#include <errno.h>
#include <inttypes.h>
#include <period_sched_group.h>
#include <stdio.h>
#include <tglock.h>
#include <time/ktime.h>
#include <trace.h>
#include <ttosBase.h>
#include <ttosProcess.h>
#include <signal_internal.h>
#include <uaccess.h>

#define KLOG_TAG "FORK"
#include <klog.h>

#undef KLOG_D
#define KLOG_D(...)

extern void kernel_set_tls(uintptr_t tls);
uintptr_t kernel_get_tls(void);
void restore_context(void *context);
void set_context_type(struct arch_context *context, int type);
int __weak arch_do_fork(pcb_t parent, pcb_t child, pcb_t caller){return 0;};

static void clone_task_entry(void *param)
{
    pcb_t pcb = ttosProcessSelf();
    assert(pcb != NULL);
    kernel_set_tls(pcb->tls);

    arch_context_set_return(&pcb->exception_context, 0);
    mm_switch_space_to(get_process_mm(pcb));
    arch_switch_context_set_stack(&pcb->taskControlId->switchContext,
                                  (long)pcb->taskControlId->kernelStackTop);

    set_context_type(&pcb->exception_context, IRQ_CONTEXT);
    restore_context(&pcb->exception_context);
}

static bool check_clone_flags(unsigned long clone_flags)
{
    if (clone_flags & CLONE_THREAD)
    {
        if (!(clone_flags & CLONE_SIGHAND))
        {
            return false;
        }
    }
    if (clone_flags & CLONE_SIGHAND)
    {
        if (!(clone_flags & CLONE_VM))
        {
            return false;
        }
    }
    return true;
}

/* 表示当前进程vfork完成 */
static void complete_vfork_done(pcb_t pcb)
{
    T_TTOS_CompletionControl *vfork;
    long flags;

    if (!pcb || !pcb_get(pcb))
    {
        KLOG_E("fail in %s, pcb:%p", __func__, pcb);
        return;
    }

    vfork = pcb->vfork_done;
    if (likely(vfork))
    {
        pcb->vfork_done = NULL;
        TTOS_ReleaseCompletion(vfork);
    }

    pcb_put(pcb);
}

void vfork_exec_wake(pcb_t pcb)
{
    complete_vfork_done(pcb);
}

void vfork_exit_wake(pcb_t pcb)
{
    complete_vfork_done(pcb);
}

pid_t do_fork(unsigned long clone_flags, unsigned long newsp, int __user *set_child_tid,
              int __user *clear_child_tid, unsigned long tls, struct period_param *param)
{
    T_TTOS_ConfigTask taskParam;
    TASK_ID child_task, parent_task;
    T_TTOS_ReturnCode ret;
    int sig_ret;
    pid_t child_pid;
    pgroup_t group;
    T_TTOS_CompletionControl vfork;
    long flags;
    bool is_period_task = false;
    pcb_t caller;
    irq_flags_t irq_flag = 0;

    if (!check_clone_flags(clone_flags))
    {
        return -1;
    }

    caller = ttosProcessSelf();

    if (caller == NULL)
    {
        return -EPERM;
    }

    pcb_t parent = caller->group_leader;
    assert(parent != NULL);
    parent_task = caller->taskControlId;
    assert(parent_task != NULL);

    /* 创建一个新的进程控制块 */
    pcb_t child = memalign(16, sizeof(struct T_TTOS_ProcessControlBlock));

    if (child == NULL)
    {
        KLOG_E("%s malloc child pcb failed, size:0x%" PRIxPTR, __func__,
               sizeof(struct T_TTOS_ProcessControlBlock));
        return -ENOMEM;
    }

    /* 拷贝进程控制块 */
    memcpy(child, caller, sizeof(*child));
    /* 初始化tglock */
    tg_lock_create(child);

    /* 重新初始化进程对象链 */
    INIT_LIST_HEAD(&child->obj_list);

    child->vfork_done = NULL;

    /* 创建一个新的进程, 属性从原来的线程复制 */
    memset(&taskParam, 0, sizeof(taskParam));
    strncpy((char *)taskParam.cfgTaskName, (const char *)parent_task->objCore.objName,
            sizeof(taskParam.cfgTaskName));
    taskParam.tickSliceSize = parent_task->tickSliceSize;
    taskParam.taskType = parent_task->taskType;
    taskParam.taskPriority = parent_task->taskPriority;
    taskParam.arg = NULL;
    taskParam.preempted = (parent_task->preemptedConfig == 0) ? true : false;
    taskParam.autoStarted = FALSE;
    taskParam.taskType = TTOS_SCHED_NONPERIOD;

    if (param)
    {
        if (param->durationTime > param->periodTime)
        {
            free(child);
            return -EINVAL;
        }

        taskParam.periodTime = param->periodTime;
        taskParam.durationTime = param->durationTime;
        taskParam.delayTime = param->delayTime;
        taskParam.taskType = TTOS_SCHED_PERIOD;
        is_period_task = true;

        /* 强制延迟启动，因为若不延迟启动，则可能在一个tick中间开始执行，任务还未执行1tick时间，tick中断就产生，导致超时*/
        if (param->autoStarted && !taskParam.delayTime)
        {
            taskParam.delayTime = 1;
        }
    }

    /* 建立新的内核栈 */
    taskParam.stackSize = parent_task->kernelStackTop - parent_task->stackBottom;
    taskParam.stackSize = taskParam.stackSize < CONFIG_KERNEL_PROCESS_STACKSIZE
                              ? CONFIG_KERNEL_PROCESS_STACKSIZE
                              : taskParam.stackSize;
    taskParam.taskStack = memalign(DEFAULT_TASK_STACK_ALIGN_SIZE, taskParam.stackSize);
    if (taskParam.taskStack == NULL)
    {
        KLOG_E("%s malloc stack failed, stack size:0x%x", __func__, taskParam.stackSize);
        return -ENOMEM;
    }
    taskParam.isFreeStack = TRUE;
    taskParam.entry = (T_TTOS_TaskRoutine)clone_task_entry;
    strcpy((char *)taskParam.objVersion, TTOS_OBJ_CONFIG_CURRENT_VERSION);

    taskParam.pcb = child;

    child->tls = kernel_get_tls();

    /* 设置新的sp */
    if (newsp != 0)
    {
        child->userStack = (void *)newsp;
        arch_context_set_stack(&child->exception_context, (long)child->userStack);

        struct mm_region *region = mm_va2region(get_process_mm(child), newsp);
        if (region == NULL)
        {
            return -EINVAL;
        }
        region->flags |= MAP_IS_STACK;
    }
    else
    {
    }

    if (clone_flags & CLONE_THREAD)
    {
        /* 同一个线程组 不创建pid */
        if (pid_obj_ref(parent, child))
        {
            return -ENOMEM;
        }
        process_wait_info_ref(parent, child);
        child->cmdline = process_obj_ref(child, parent->cmdline);
        child->parent = NULL;
        child->sibling = NULL;
        child->first_child = NULL;
        KLOG_D("pcb:%p create thread pcb:%p", parent, child);

        arch_context_thread_init(&child->exception_context);
        taskParam.isprocess = FALSE;

        // task_create_procfs_dir(get_process_pid(child), child->taskControlId->tid);
    }
    else
    {
        /* 创建新的pid */
        child->pid = pid_obj_alloc(child);
        if (child->pid == NULL)
        {
            KLOG_E("%s %d:alloc pid failed", __func__, __LINE__);
            return -ENOMEM;
        }

        child->cmdline =
            process_obj_create(child, strdup((char *)parent->cmdline->really->obj), free);

        if (child->cmdline == NULL)
        {
            return -ENOMEM;
        }

        process_wait_info_create(child);
        if (child->wait_info == NULL)
        {
            KLOG_E("%s %d:alloc wait_info failed", __func__, __LINE__);
            return -ENOMEM;
        }

        KLOG_D("pcb:%p create pcb:%p", parent, child);
        /* 只有完整进程才存在父子关系 */
        child->parent = parent;
        child->sibling = parent->first_child;
        parent->first_child = child;
        child->first_child = NULL;
        taskParam.isprocess = TRUE;

        // task_create_procfs_dir(get_process_pid(child), get_process_pid(child));
    }

    pcb_flags_set(child, FORKNOEXEC);

    /* 复制文件列表 */
    if (clone_flags & CLONE_FILES)
    {
        process_filelist_ref(parent, child);
    }
    else
    {
        process_filelist_copy(parent, child);
    }

    if (clone_flags & CLONE_VM)
    {
        /* 共享mmu表 */
        process_mm_ref(parent, child);
    }
    else
    {
        /* 复制整个mmu表 */
        process_mm_copy(parent, child);
        /* 设置新的 ASID */
        get_process_mm(child)->asid = get_process_pid(child);
    }

    /* execve 了才换 不需要考虑是否是同一进程组 */
    child->envp = process_obj_ref(child, parent->envp);

    if (clone_flags & CLONE_PARENT_SETTID)
    {
        child->set_child_tid = set_child_tid;
    }
    else
    {
        child->set_child_tid = NULL;
    }

    if (clone_flags & CLONE_SETTLS)
    {
        child->tls = tls;
    }

    if (clone_flags & CLONE_CHILD_CLEARTID)
    {
        child->clear_child_tid = clear_child_tid; /* 子进程退出时，清除子进程的线程 ID */
    }

    /* 初始化线程组链表 */
    INIT_LIST_HEAD(&(child->thread_group));

    if (clone_flags & CLONE_THREAD)
    {
        child->tgid = parent->tgid;
        child->group_leader = parent;
        tg_lock(parent, &irq_flag);
        list_add_after(&child->sibling_node, &parent->thread_group);
        tg_unlock(parent, &irq_flag);
    }
    else
    {
        /* 查找父进程所在的进程组 */
        group = process_pgrp_find(process_pgid_get_byprocess(parent));
        if (group)
        {
            /* 子进程加入进程组 */
            pgrp_insert(group, child);
            pgrp_put(group);
        }
        else
        {
            KLOG_E("fail at %s:%d", __FILE__, __LINE__);
        }

        /* 主线程为线程组leader */
        child->group_leader = child;

        /* leader的tgid和其pid相同 */
        child->tgid = get_process_pid(child);

        tg_lock(child, &irq_flag);
        list_add(&child->sibling_node, &child->thread_group);
        tg_unlock(child, &irq_flag);
    }

    sig_ret = fork_signal(clone_flags, child);
    if (sig_ret)
    {
        /*
         * 这里先只记录失败，不在 fork 路径里临时拼一套不完整的回滚逻辑。
         * 等编译/运行环境恢复后，可以结合日志继续把 child 半初始化清理补完整。
         */
        KLOG_E("fork_signal failed, child=%p parent=%p clone_flags=0x%lx ret=%d",
               child, parent, clone_flags, sig_ret);
    }

    /* 继承父进程的控制终端 */
    signal_shared_get(child)->ctty = signal_shared_get(parent)->ctty;

    /* 创建pcb自旋锁 */
    INIT_SPIN_LOCK(&child->lock);

    if (is_period_task)
    {
        char group_name[128] = {0};
        sprintf(group_name, "group_period:%u", param->periodTime);
        int sched_group_id = period_sched_group_create(param->periodTime, group_name);
        period_sched_group_add(child, sched_group_id);
    }

    child_pid = get_process_pid(child);

    /* 非周期任务 || 自启动的周期任务 */
    if (!is_period_task || (is_period_task && param->autoStarted))
    {
        /* 创建新任务 */
        ret = TTOS_CreateTask(&taskParam, &child_task);
        if (ret != TTOS_OK)
        {
            KLOG_E("create task error %d", ret);
            return -ENOMEM;
        }

        if (child->set_child_tid != NULL)
        {
            copy_to_user(child->set_child_tid, &child->taskControlId->tid, sizeof(int));
        }

        trace_fork(child_task);
        TTOS_ActiveTask(child_task);
    }
    else
    {
        if (child->set_child_tid != NULL)
        {
            copy_to_user(child->set_child_tid, &child->taskControlId->tid, sizeof(int));
        }

        trace_fork(child->taskControlId);
        /* 非自启动的周期任务 */
        wait_actived_task_add(child->taskControlId->tid);
    }

    arch_do_fork(parent, child, caller);

    if (clone_flags & CLONE_VFORK)
    {
        child->vfork_done = &vfork;
        TTOS_InitCompletion(&vfork);

        /* 等待vfork创建的子进程退出或执行execve */
        TTOS_WaitCompletionUninterruptible(&vfork, TTOS_WAIT_FOREVER);
    }
    else
    {
        ttosRotateRunningTask(ttosGetRunningTask());
    }

    return child_pid;
}
