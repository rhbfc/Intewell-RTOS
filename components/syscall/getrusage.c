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
#include <spinlock.h>
#include <tglock.h>
#include <time/ktime.h>
#include <ttosProcess.h>
#include <uaccess.h>

/**
 * @brief 原子读取单个任务的时间统计。
 *
 * 在多核环境下，必须在中断关闭的情况下读取时间数据，
 * 以避免时间戳被调度器同时更新导致的数据撕裂。
 *
 * @param[in] thread 线程控制块
 * @param[in,out] ru 资源使用统计结构体
 */
static inline void __add_task_rusage_atomic(pcb_t thread, struct rusage *ru)
{
    unsigned long flags;

    ttos_int_lock(flags);
    {
        ru->ru_utime.tv_sec += thread->utime.tv_sec;
        ru->ru_utime.tv_usec += thread->utime.tv_nsec / NSEC_PER_USEC;
        ru->ru_stime.tv_sec += thread->stime.tv_sec;
        ru->ru_stime.tv_usec += thread->stime.tv_nsec / NSEC_PER_USEC;
    }
    ttos_int_unlock(flags);
}

/**
 * @brief 获取单个进程的资源使用统计（包含所有线程）。
 *
 * 在线程组锁的保护下遍历所有线程，并原子地读取每个线程的时间统计。
 * 内存区域统计单独在内存锁的保护下进行。
 *
 * @param[in] group_leader 进程组的首线程
 * @param[in,out] ru 资源使用统计结构体
 */
static void __getrusage_process_locked(pcb_t group_leader, struct rusage *ru)
{
    pcb_t thread;
    struct mm *mm;
    struct mm_region *region;
    irq_flags_t flags;

    /* 在线程组锁保护下，遍历所有线程并原子读取时间戳 */
    tg_lock(group_leader, &flags);
    list_for_each_entry(thread, &group_leader->thread_group, sibling_node)
    {
        __add_task_rusage_atomic(thread, ru);
    }
    tg_unlock(group_leader, &flags);

    /* 获取内存区域大小（在mm锁保护下） */
    mm = get_process_mm(group_leader);
    if (mm != NULL)
    {
        unsigned long flags = read_lock_irqsave(&mm->mm_region_lock);
        list_for_each_entry(region, &mm->mm_region_list, list)
        {
            ru->ru_maxrss += region->region_page_count << PAGE_SIZE_SHIFT;
        }
        read_unlock_irqrestore(&mm->mm_region_lock, flags);
    }
}

/**
 * @brief 获取子进程的资源使用统计（内部用）。
 *
 * 这是一个helper函数，在foreach_process_child的迭代中被调用。
 * 负责对单个子进程执行完整的资源统计收集。
 *
 * @param[in] child 子进程控制块
 * @param[in,out] param 指向rusage结构体的指针
 */
static void __collect_child_rusage(pcb_t child, void *param)
{
    struct rusage *ru = (struct rusage *)param;

    /* 子进程的资源统计与父进程相同的方式获取 */
    __getrusage_process_locked(child, ru);
}

/**
 * @brief 系统调用实现：获取进程资源使用统计信息。
 *
 * 该函数实现了一个系统调用，用于获取指定进程或线程的资源使用统计信息，
 * 包括CPU时间、内存使用等。
 *
 * 多核安全性：
 * - 时间戳读取在中断关闭的状态下进行，防止调度器在读取时更新数据
 * - 线程遍历在线程组互斥锁保护下进行
 * - 内存区域遍历在内存自旋锁保护下进行
 * - 锁的获取顺序固定为：tg_lock → mm_lock，防止死锁
 *
 * @param[in] who 目标类型：
 *                - RUSAGE_SELF：当前进程
 *                - RUSAGE_CHILDREN：子进程
 *                - RUSAGE_THREAD：当前线程
 * @param[out] ru 资源使用统计结构体：
 *                - ru_utime：用户态CPU时间
 *                - ru_stime：内核态CPU时间
 *                - ru_maxrss：最大常驻集大小
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功获取统计信息。
 * @retval -EINVAL who参数无效。
 * @retval -EFAULT ru指向无效内存。
 *
 * @note 1. 统计信息包括CPU时间和内存使用。
 *       2. 支持进程和线程级统计。
 *       3. 支持子进程统计。
 *       4. 时间精度到微秒。
 */
DEFINE_SYSCALL(getrusage, (int who, struct rusage __user *ru))
{
    struct rusage kru;
    pcb_t pcb = ttosProcessSelf();

#ifdef SYS_getrusage_time64
    if (!user_access_check(ru, sizeof(long long[18]), UACCESS_W))
    {
        return -EFAULT;
    }
#else
    if (!user_access_check(ru, sizeof(long[4]), UACCESS_W))
    {
        return -EFAULT;
    }
#endif

    memset(&kru, 0, sizeof(struct rusage));

    switch (who)
    {
    case RUSAGE_SELF:
        /* 当前进程的资源统计包括所有线程 */
        __getrusage_process_locked(pcb->group_leader, &kru);
        break;

    case RUSAGE_THREAD:
        /* 仅当前线程的统计 */
        __add_task_rusage_atomic(pcb, &kru);
        break;

    case RUSAGE_CHILDREN:
        /* 所有子进程的统计，通过foreach_process_child迭代调用 */
        foreach_process_child(pcb, __collect_child_rusage, &kru);
        break;

    default:
        return -EINVAL;
    }

#ifdef SYS_getrusage_time64
    if (copy_to_user(ru, &kru, sizeof(long long[18])) != 0)
    {
        return -EFAULT;
    }
#else
    long kru_time[4];
    kru_time[0] = kru.ru_utime.tv_sec;
    kru_time[1] = kru.ru_utime.tv_usec;
    kru_time[2] = kru.ru_stime.tv_sec;
    kru_time[3] = kru.ru_stime.tv_usec;
    if (copy_to_user(ru, kru_time, sizeof(long) * 4) != 0)
    {
        return -EFAULT;
    }
#endif

    return 0;
}