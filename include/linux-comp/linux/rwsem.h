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

#ifndef __RWSEAM_H__
#define __RWSEAM_H__
#include <atomic.h>
#include <errno.h>
#include <stdbool.h>
#include <ttos.h>
#include <ttosBase.h>

struct rw_semaphore
{
    atomic_t count;
    T_TTOS_Task_Queue_Control wait;
};
#define init_rwsem(s)                                                                              \
    do                                                                                             \
    {                                                                                              \
        ttosInitializeTaskq(&(s)->wait, T_TTOS_QUEUE_DISCIPLINE_FIFO,                              \
                            TTOS_TASK_WAITING_FOR_SEMAPHORE);                                      \
        ATOMIC_INIT(&((s)->count), 0);                                                             \
    } while (0);

static inline int _down_read(struct rw_semaphore *sem, bool is_interruptable)
{
    long old;
    while (1)
    {
        /* 读取计数 只有当计数大于等于0时才可以获取锁 */
        old = atomic_read(&sem->count);
        /* 原子+1, 需要确保不是-1加上来的    */
        if (old >= 0 && atomic_cas(&sem->count, old, old + 1))
        {
            break;
        }
        /* 当计数小于0时, 说明有写锁, 进入等待队列挂起 */
        if (old < 0)
        {
            ttosDisableTaskDispatchWithLock();
            ttosGetRunningTask()->wait.returnCode = TTOS_OK;

            ttosEnqueueTaskq(&sem->wait, TTOS_WAIT_FOREVER, is_interruptable);
            ttosEnableTaskDispatchWithLock();
            if (ttos_ret_to_errno(ttosGetRunningTask()->wait.returnCode) == EINTR)
            {
                return -EINTR;
            }
        }
    }
    return 0;
}

static inline void _weak_up_rw_task(struct rw_semaphore *sem)
{
    struct list_node *node = NULL, *next_node = NULL;
    T_TTOS_TaskControlBlock *task;

    /* @KEEP_COMMENT: 禁止调度器 */
    ttosDisableTaskDispatchWithLock();

    node = list_first(&(sem->wait.queues.fifoQueue));

    while (node)
    {
        next_node = list_next(node, &(sem->wait.queues.fifoQueue));
        task = (T_TTOS_TaskControlBlock *)(((T_TTOS_ResourceTaskNode *)node)->task);
        list_delete(node);

        if ((task != NULL))
        {
            /* @REPLACE_BRACKET: node任务在任务tick等待队列上 */
            if (task->objCore.objectNode.next != NULL)
            {
                /* @KEEP_COMMENT:
                 * 调用ttosExactWaitedTask(DT.2.24)将node任务从任务tick等待队列中移除
                 */
                (void)ttosExactWaitedTask(task);
            }

            (void)ttosClearTaskWaiting(task);
            task->wait.queue = NULL;
            task->wait.returnCode = TTOS_OK;
        }
        node = next_node;
    }

    /* @KEEP_COMMENT: 重新使能调度 */
    ttosEnableTaskDispatchWithLock();
}

static inline void up_read(struct rw_semaphore *sem)
{
    long old;
    while (1)
    {
        /* 读取计数 只有当计数大于等于1时才可以释放锁 */
        old = atomic_read(&sem->count);
        /* TODO 如果此时old等于0表示此函数的调用流程错误, 应该进行错误处理 */

        /* 原子-1, 需要确保不是-1减下来的    */
        if (old > 0 && atomic_cas(&sem->count, old, old - 1))
        {
            old = atomic_read(&sem->count);

            /* 当old为0 时尝试唤醒所有等待的任务,如果存在的话 */
            if (old == 0)
            {
                _weak_up_rw_task(sem);
            }
            break;
        }
    }
}

static inline int _down_write(struct rw_semaphore *sem, bool is_interruptable)
{
    long old;

    while (1)
    {
        /* 读取计数 只有当计数为0时才可以获取锁 */
        old = atomic_read(&sem->count);

        /* 当计数不为0时,进入等待队列挂起 */
        while (old != 0)
        {
            ttosDisableTaskDispatchWithLock();
            ttosGetRunningTask()->wait.returnCode = TTOS_OK;

            ttosEnqueueTaskq(&sem->wait, TTOS_WAIT_FOREVER, is_interruptable);
            ttosEnableTaskDispatchWithLock();

            if (ttos_ret_to_errno(ttosGetRunningTask()->wait.returnCode) == EINTR)
            {
                return -EINTR;
            }
            old = atomic_read(&sem->count);
        }

        /* 原子操作将计数值减为-1 但必须是从0减下来的,如果失败则需要重新开始整个过程 */
        if (atomic_cas(&sem->count, old, -1))
        {
            /* 此时才算持锁 */
            return 0;
        }
    }
}

static inline void up_write(struct rw_semaphore *sem)
{
    long old;
    while (1)
    {
        /* 读取计数 只有当计数大于等于-1时才可以释放锁 */
        old = atomic_read(&sem->count);

        /* TODO 如果此时old不等于-1表示此函数的调用流程错误, 应该进行错误处理 */

        /* 原子-1, 需要确保不是-1减下来的    */
        if (old < 0 && atomic_cas(&sem->count, old, 0))
        {
            old = atomic_read(&sem->count);

            /* 当old为0 时尝试唤醒所有等待的任务,如果存在的话 */
            if (old == 0)
            {
                _weak_up_rw_task(sem);
            }
            break;
        }
    }
}

#define down_read(s) _down_read(s, false)
#define down_read_killable(s) _down_read(s, true)
#define down_write(s) _down_write(s, false)
#define down_write_killable(s) _down_write(s, true)

#endif /* __RWSEAM_H__ */
