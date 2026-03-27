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

#ifndef _RT_MUTEX_H_
#define _RT_MUTEX_H_

#include <list.h>
#include <spinlock.h>
#include <ttosRBTree.h>

typedef struct T_TTOS_TaskControlBlock_Struct *TASK_ID;

struct rt_mutex_waiter
{
    TASK_ID task;

    int prio_snapshot; // 插入时 eff_prio，用于检测变化
    struct rb_node rb;
};

struct rt_mutex
{
    TASK_ID owner;

    struct rb_root waiters;       // 按 eff_prio 排序
    ttos_spinlock_t waiters_lock; // 保护 owner/waiters/top_waiter

    struct rt_mutex_waiter *top_waiter; // 缓存最高优先级 waiter（O(1)）

    struct list_head owner_node; // 挂到 owner->held_rt_mutexs
};

void waiter_requeue_if_needed(struct rt_mutex *m, struct rt_mutex_waiter *w);
void rt_mutex_waiter_prio_changed(TASK_ID t);
struct rt_mutex *rt_mutex_create(void);
int rt_mutex_init(struct rt_mutex *m);
int rt_mutex_destroy(struct rt_mutex *m);
int rt_mutex_lock(struct rt_mutex *m);
int rt_mutex_lock_timeout(struct rt_mutex *m, unsigned int ticks);
int rt_mutex_enqueue_waiter(struct rt_mutex *m, TASK_ID t);
int rt_mutex_unlock(struct rt_mutex *m);
int rt_mutex_unlock_prepare(struct rt_mutex *m, TASK_ID *next);
void rt_mutex_unlock_finish(struct rt_mutex *m, TASK_ID next);
int rt_mutex_take_top_waiter(struct rt_mutex *m, TASK_ID *next);
void rt_mutex_wake_owner(struct rt_mutex *m, TASK_ID next);
void rt_mutex_waiter_abort(TASK_ID t);

#endif /* _RT_MUTEX_H_ */
