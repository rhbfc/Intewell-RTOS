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

#include "syscall_internal.h"
#include <arch_types.h>
#include <assert.h>
#include <errno.h>
#include <pid.h>
#include <process_exit.h>
#include <signal_internal.h>
#include <tasklist_lock.h>
#include <tglock.h>
#include <ttos.h>
#include <ttosInterTask.inl>
#include <ttosProcess.h>
#include <uaccess.h>

#undef KLOG_TAG
#undef KLOG_LEVEL
#define KLOG_LEVEL KLOG_INFO
#define KLOG_TAG "SYS_WAIT4"
#include <klog.h>

#undef KLOG_D
#define KLOG_D(...)

static int do_waitpid(pid_t pid, int __user *stat_addr, int options, struct rusage __user *ru);

enum wait_event_type
{
    WAIT_EVENT_NONE = 0,
    WAIT_EVENT_EXIT,
    WAIT_EVENT_STOP,
    WAIT_EVENT_CONTINUE,
};

/**
 * @brief 等待子进程状态改变。
 *
 * 此函数实现了一个系统调用，用于等待指定的子进程状态发生改变。
 * 状态改变包括：子进程终止、被信号停止或被信号恢复。
 *
 * @param[in] pid 要等待的进程ID：
 *               - < -1：等待进程组ID为-pid的任一子进程
 *               - -1：等待任一子进程
 *               - 0：等待与调用进程同组的任一子进程
 *               - > 0：等待指定进程ID的子进程
 * @param[out] stat_addr 子进程状态信息的存储地址
 * @param[in] options 选项标志：
 *                  - WNOHANG：如果没有子进程退出则立即返回
 *                  - WUNTRACED：报告被停止的子进程
 *                  - WCONTINUED：报告被恢复的子进程
 *                  - __WNOTHREAD：只等待调用线程的子进程
 *                  - __WCLONE：等待克隆的子进程
 *                  - __WALL：等待所有子进程
 * @param[out] ru 用于返回子进程的资源使用统计信息
 * @return 成功时返回子进程ID，失败时返回负值错误码。
 * @retval > 0 子进程的进程ID。
 * @retval 0 使用WNOHANG且没有子进程状态改变。
 * @retval -ECHILD 没有子进程。
 * @retval -EINVAL 无效的选项标志。
 * @retval -EFAULT 参数指向无效内存。
 *
 * @note 1. 此函数会阻塞直到有子进程状态改变或收到信号。
 *       2. stat_addr中返回的状态可以用WIFEXITED等宏解析。
 *       3. 如果指定ru参数，将返回子进程的资源使用统计。
 *       4. 此函数是waitpid的增强版本，提供了更多功能。
 */
DEFINE_SYSCALL(wait4, (pid_t pid, int __user *stat_addr, int options, struct rusage __user *ru))
{
    if (options & ~(WNOHANG | WUNTRACED | WCONTINUED | __WNOTHREAD | __WCLONE | __WALL))
        return -EINVAL;

    KLOG_D("wait4, pid = %d, self = %p[%d]---> %s", pid, ttosProcessSelf(),
           get_process_pid(ttosProcessSelf()), ttosProcessSelf()->cmd_name);

    return do_waitpid(pid, stat_addr, options, ru);
}

/**
 * @brief 设置等待子进程的退出状态。
 *
 * 此函数根据子进程的退出代码和退出信号设置其状态。
 *
 * @param[in] pcb 进程控制块指针
 * @return 返回格式化后的退出状态。
 * @retval > 0 正常退出的状态值。
 * @retval 0 进程未退出。
 *
 * @note 1. exit_code和exit_signal不能同时非零。
 *       2. 状态值可以用WIFEXITED等宏解析。
 */
static int set_waitpid_status(pcb_t pcb, enum wait_event_type event_type)
{
    assert(pcb);
    int exit_code, exit_signal;

    if (event_type == WAIT_EVENT_CONTINUE)
    {
        pcb->state = 0;
        return PROCESS_CREATE_STAT_CONTINUED;
    }

    exit_code = pcb->exit_code;
    exit_signal = pcb->exit_signal;

    KLOG_D("%s, exit_code = %d, exit_signal = %d", __func__, pcb->exit_code, pcb->exit_signal);

    /* exit_code与exit_signal 二者不能同时不为0 */
    assert((exit_code == 0 || exit_signal == 0) && !(exit_code != 0 && exit_signal != 0));

    /* 正常退出 */
    if (exit_code)
    {
        pcb->exit_code = 0;
        return PROCESS_CREATE_STAT_EXIT(exit_code);
    }

    /* 信号退出 */
    if (exit_signal)
    {
        pcb->exit_signal = 0;

        if (event_type == WAIT_EVENT_STOP || is_stop_set(exit_signal))
        {
            return PROCESS_CREATE_STAT_STOPPED(exit_signal);
        }

        return PROCESS_CREATE_STAT_SIGNALED(exit_signal);
    }

    return 0;
}

static bool wait_status_is_stopped(pcb_t pcb)
{
    return pcb->exit_signal > 0 && is_stop_set(pcb->exit_signal);
}

static bool wait_status_is_continued(pcb_t pcb)
{
    return pcb->state == PROCESS_CREATE_STAT_CONTINUED;
}

static enum wait_event_type wait_status_event_type(pcb_t pcb, wait_handle_t *wait_handle)
{
    if (!pcb)
    {
        return WAIT_EVENT_NONE;
    }

    if (pcb->exit_state == EXIT_ZOMBIE)
    {
        return WAIT_EVENT_EXIT;
    }

    if ((wait_handle->options & WUNTRACED) && wait_status_is_stopped(pcb))
    {
        /* 普通 job-control stop 由 signal_notify_parent_and_leader() 产出。 */
        return WAIT_EVENT_STOP;
    }

    if ((wait_handle->options & WCONTINUED) && wait_status_is_continued(pcb))
    {
        /* continued 事件只上报一次，set_waitpid_status() 会负责消费它。 */
        return WAIT_EVENT_CONTINUE;
    }

    return WAIT_EVENT_NONE;
}

static bool wait_status_is_reportable(pcb_t pcb, wait_handle_t *wait_handle)
{
    return wait_status_event_type(pcb, wait_handle) != WAIT_EVENT_NONE;
}

/* with tasklist_lock hold */
pcb_t wait_thread_zombie(pcb_t pcb, wait_handle_t *wait_handle)
{
    irq_flags_t flags;
    pcb_t zombie_found = NULL, target;
    pid_t pid = wait_handle->pid;
    bool ret = false;

    if (pid > 0)
    {
        ret = wait_status_is_reportable(pcb, wait_handle);
        if (ret)
            zombie_found = pcb;
    }
    else
    {
        /* 此时传入的pcb为self，检查当前线程组的child的，任意子线程是否已经退出 */
        tg_lock(pcb->group_leader, &flags);
        list_for_each_entry(target, &pcb->group_leader->thread_group, sibling_node)
        {
            ret = wait_status_is_reportable(target, wait_handle);
            if (ret)
            {
                zombie_found = target;
                break;
            }
        }

        tg_unlock(pcb->group_leader, &flags);
    }

    return zombie_found;
}

/**
 * @brief 等待进程组中任一进程退出。
 *
 * 此函数在指定进程组中查找任一进程是否已经退出。
 *
 * @param[in] wait_handle 等待句柄
 * @return 返回退出的进程控制块指针，否则返回NULL。
 *
 * @note 1. 此函数需要持有进程组锁。
 *       2. 如果找到退出的进程，将返回其控制块指针。
 */
static pcb_t wait_pgrp_zombie(wait_handle_t *wait_handle)
{
    pid_t pid;
    pgroup_t pgroup;
    pcb_t pos;
    pcb_t target = NULL;

    pid = wait_handle->pid;

    /* pid = 0 或 pid < -1, 等待指定线程组的任意线程退出 */
    pgroup = process_pgrp_find(-pid);
    if (!pgroup)
    {
        return NULL;
    }

    /* 进入等待之前检查，进程组内是否存在符合要求的进程退出 */
    TTOS_ObtainMutex(pgroup->lock, TTOS_WAIT_FOREVER);

    list_for_each_entry(pos, &(pgroup->process), pgrp_node)
    {
        if (wait_status_is_reportable(pos, wait_handle))
        {
            target = pos;
            break;
        }
    }

    TTOS_ReleaseMutex(pgroup->lock);
    pgrp_put(pgroup);

    return target;
}

/**
 * 该函数用于在进程进入等待状态之前，检测是否已经有符合要求的，已经退出或处于僵尸态的进程；
 * 如果有，父进程将跳过等待，直接返回已经退出的进程，由外层执行收割；
 * 否则 父进程在持有tasklist_lock的情况下进入等待
 *
 */
pcb_t wait_process_zombie(pcb_t pcb, wait_handle_t *wait_handle)
{
    bool ret = false;
    pid_t pid;
    pcb_t waker_pcb = NULL;

    pid = wait_handle->pid;

    if (pid > 0)
    {
        ret = wait_status_is_reportable(pcb, wait_handle);
    }
    else if (pid == -1)
    {
        /**
         * 某一个子进程退出，收集退出子进程的信息;
         * 此时传入的pcb参数为group_leader->first_child
         */
        while (pcb != NULL)
        {
            ret = wait_status_is_reportable(pcb, wait_handle);
            if (ret)
                break;

            pcb = pcb->sibling;
        }
    }
    else
    {
        /* 等待进程组，判断是否有进程组中的进程已经退出 */
        waker_pcb = wait_pgrp_zombie(wait_handle);

        /* 直接返回，避免将pcb赋值给waker_pcb */
        ret = false;
    }

    if (ret)
    {
        waker_pcb = pcb;
    }

    return waker_pcb;
}

/**
 * @brief 等待进程退出并释放资源。
 *
 * 此函数等待指定进程退出，并释放其资源。
 *
 * @param[in] target 进程控制块指针
 * @param[in] wait_handle 等待句柄
 * @return 返回0表示成功，否则返回错误码。
 *
 * @note 1. 此函数需要持有tasklist_lock。
 *       2. 如果进程已经退出，将释放其资源。
 */
static int wait_and_release(pcb_t target, wait_handle_t *wait_handle)
{
    int ret = 0;
    pcb_t self = ttosProcessSelf();
    pcb_t pcb_waker;
    enum wait_event_type wait_event = WAIT_EVENT_NONE;

    if (!target)
    {
        return -ECHILD;
    }

    do
    {
        ret = wait_process(target, wait_handle);

        /* 目标进程没有退出，且设置了WNOHANG，直接返回 */
        if (ret == -EAGAIN && (wait_handle->options & WNOHANG))
        {
            wait_handle->stat.pid = 0;
            wait_handle->stat.exit_status = 0;
            return 0;
        }

        /* pid > 0 时，等待指定进程，但是指定进程此时没有退出或进入trace状态暂停，将再次阻塞等待 */
        if (wait_handle->pid > 0)
            pcb_waker = target;
        else
            pcb_waker = get_process_waitinfo(self)->waker;

        /* 检查等待的目标进程是否已经退出 */
        if (pcb_waker)
        {
            wait_event = wait_status_event_type(pcb_waker, wait_handle);
            if (wait_event != WAIT_EVENT_NONE)
            {
                break;
            }
        }
        else
        {
            /* 异常唤醒，且没有符合条件的进程 */
            return ret;
        }

    } while (1);

    /* 找到符合要求的进程，忽略INTR或其他错误标志位，返回0给上一级 */
    ret = 0;

    /* 保存退出信息，收割唤醒自身的那个进程或线程 */
    wait_handle->stat.exit_status = set_waitpid_status(pcb_waker, wait_event);
    wait_handle->stat.pid = get_process_pid(pcb_waker);

    if (pcb_waker->exit_state == EXIT_ZOMBIE)
        process_release_zombie(pcb_waker);

    KLOG_D("wait res: pid %d, waiter %p[%d]-->%s", wait_handle->stat.pid, ttosProcessSelf(),
           get_process_pid(ttosProcessSelf()), ttosProcessSelf()->taskControlId->objCore.objName);

    return ret;
}

/**
 * @brief 等待进程退出。
 *
 * 此函数等待指定进程退出，并返回其退出状态。
 *
 * @param[in] wait_handle 等待句柄
 * @return 返回0表示成功，否则返回错误码。
 *
 * @note 1. 此函数需要持有tasklist_lock。
 *       2. 如果进程已经退出，将返回其退出状态。
 */
static int do_wait_process(wait_handle_t *wait_handle)
{
    pcb_t target = NULL;
    pcb_t self;
    pid_t pid;
    pgroup_t pgroup;

    self = ttosProcessSelf();
    pid = wait_handle->pid;
    if (pid > 0)
    {
        /* pid 大于0, 等待指定线程 */
        if (pid >= CONFIG_PROCESS_MAX)
        {
            /* 根据线程ID获取任务控制块 */
            TASK_ID tcb = task_get_by_tid(pid);
            /* 如果任务控制块存在 */
            if (tcb)
            {
                /* 获取任务对应的线程pcb */
                target = (pcb_t)tcb->ppcb;
            }
        }
        else
        {
            /* 根据进程ID获取进程控制块 */
            target = pcb_get_by_pid(pid);
        }

        if (!target)
        {
            KLOG_D("%s, Invalid pid %d", __func__, pid);
            return -ECHILD;
        }

        return wait_and_release(target, wait_handle);
    }
    else if (pid == -1)
    {
        /* pid == -1, 等待当前进程的任意子进程 */
        return wait_and_release(self->first_child, wait_handle);
    }
    else
    {
        /*
         * 这里继续复用 self 作为 wait_and_release() 的入口；
         * 进程组维度真正的可见状态仍由后续 wait_process()/waker 逻辑决定。
         */

        /* pid = 0 或 pid < -1, 等待指定线程组的任意线程退出 */
        pgroup = process_pgrp_find(-pid);
        if (!pgroup)
        {
            KLOG_E("%s:%d invalid pgid %d", __FILE__, __LINE__, -pid);
            return -ESRCH;
        }

        /* pgroup 的长期有效性仍沿用现有生命周期管理约束。 */
        pgrp_put(pgroup);

        return wait_and_release(self, wait_handle);
    }

}

/**
 * @brief 等待线程退出。
 *
 * 此函数等待指定线程退出，并返回其退出状态。
 *
 * @param[in] wait_handle 等待句柄
 * @return 返回0表示成功，否则返回错误码。
 *
 * @note 1. 此函数需要持有tasklist_lock。
 *       2. 如果线程已经退出，将返回其退出状态。
 */
static int do_wait_thread(wait_handle_t *wait_handle)
{
    pcb_t self, target = NULL;
    pid_t pid = wait_handle->pid;

    self = ttosProcessSelf();

    /* pid 大于0, 等待指定线程 */
    if (pid >= CONFIG_PROCESS_MAX)
    {
        /* 根据线程ID获取任务控制块 */
        TASK_ID tcb = task_get_by_tid(pid);
        /* 如果任务控制块存在 */
        if (tcb)
        {
            /* 获取任务对应的线程pcb */
            target = (pcb_t)tcb->ppcb;
        }

        if (!target)
        {
            KLOG_D("%s, invalid pid %d", __func__, pid);
            return -ECHILD;
        }

        KLOG_D("%s, wait pid %d", __func__, pid);

        return wait_and_release(target, wait_handle);
    }
    else
    {
        /* 在self的，每一个child 的 线程组中找符合要求的 */
        return wait_and_release(self, wait_handle);
    }

}

/**
 * @brief 等待进程或线程退出。
 *
 * 此函数等待指定进程或线程退出，并返回其退出状态。
 *
 * @param[in] wait_handle 等待句柄
 * @return 返回0表示成功，否则返回错误码。
 *
 * @note 1. 此函数需要持有tasklist_lock。
 *       2. 如果进程或线程已经退出，将返回其退出状态。
 */
static int do_wait(wait_handle_t *wait_handle)
{
    int ret = 0;

    /* 如果指定了__WALL，在wait_process返回失败的情况下，继续等待线程 */
    if (wait_handle->options & __WALL)
    {
        KLOG_D("wall");
        ret = do_wait_thread(wait_handle);
    }
    else
    {
        KLOG_D("no wall");
        /* 没有指定__WALL，只等待有父子关系的进程 */
        ret = do_wait_process(wait_handle);
    }

    return ret;
}

/**
 * @brief 等待子进程状态改变。
 *
 * 此函数等待指定子进程状态改变，并返回其退出状态。
 *
 * @param[in] pid 要等待的进程ID：
 *               - < -1：等待进程组ID为-pid的任一子进程
 *               - -1：等待任一子进程
 *               - 0：等待与调用进程同组的任一子进程
 *               - > 0：等待指定进程ID的子进程
 * @param[out] stat_addr 子进程状态信息的存储地址
 * @param[in] options 选项标志：
 *                  - WNOHANG：如果没有子进程退出则立即返回
 *                  - WUNTRACED：报告被停止的子进程
 *                  - WCONTINUED：报告被恢复的子进程
 *                  - __WNOTHREAD：只等待调用线程的子进程
 *                  - __WCLONE：等待克隆的子进程
 *                  - __WALL：等待所有子进程
 * @param[out] ru 用于返回子进程的资源使用统计信息
 * @return 成功时返回子进程ID，失败时返回负值错误码。
 * @retval > 0 子进程的进程ID。
 * @retval 0 使用WNOHANG且没有子进程状态改变。
 * @retval -ECHILD 没有子进程。
 * @retval -EINVAL 无效的选项标志。
 * @retval -EFAULT 参数指向无效内存。
 *
 * @note 1. 此函数会阻塞直到有子进程状态改变或收到信号。
 *       2. stat_addr中返回的状态可以用WIFEXITED等宏解析。
 *       3. 如果指定ru参数，将返回子进程的资源使用统计。
 *       4. 此函数是waitpid的增强版本，提供了更多功能。
 */
static int do_waitpid(pid_t pid, int __user *stat_addr, int options, struct rusage __user *ru)
{
    /* 返回值初始化为0 */
    int ret = 0;

    /* 定义进程控制块指针和当前进程指针 */
    pcb_t self;
    wait_handle_t wait_handle;
    unsigned int exit_status = 0, exit_pid = 0;

    /* 获取当前进程的进程控制块 */
    self = ttosProcessSelf();

    assert(self != NULL);


    wait_handle.options = options;
    wait_handle.pid = pid;
    wait_handle.stat_addr = stat_addr;
    wait_handle.ru = ru;
    wait_handle.options |= WEXITED;

    /* 执行等待、收割僵尸进程的操作 */
    ret = do_wait(&wait_handle);
    if (ret)
    {
        return ret;
    }

    exit_pid = wait_handle.stat.pid;
    exit_status = wait_handle.stat.exit_status;

    /* 拷贝退出状态信息 */
    if (stat_addr)
    {
        copy_to_user(stat_addr, &exit_status, sizeof(int));
    }

    KLOG_D("waitpid return %d, status 0x%x", exit_pid, exit_status);
    return exit_pid;
}

static void wait_info_destroy(void *obj)
{
    free(obj);
}

void process_wait_info_create(pcb_t pcb)
{
    struct waitpid_info *waitinfo = calloc(1, sizeof(struct waitpid_info));

    waitinfo->status = 0;
    waitinfo->waker = NULL;
    (void)ttosInitializeTaskq(&waitinfo->waitQueue, T_TTOS_QUEUE_DISCIPLINE_FIFO,
                              TTOS_TASK_WAITING_FOR_PROCESS);

    pcb->wait_info = process_obj_create(pcb, waitinfo, wait_info_destroy);
}

void process_wait_info_ref(pcb_t parent, pcb_t child)
{
    child->wait_info = process_obj_ref(child, parent->wait_info);
}
