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
#include <kmalloc.h>
#include <ttos.h>
#include <ttosBase.h>
#include <uaccess.h>
#include <system/bug.h>

static bool rt_mutex_task_alive(TASK_ID t)
{
    if (!t)
    {
        return false;
    }

    return t->taskControlId == t;
}

static void rt_mutex_owner_list_add_safe(TASK_ID owner, struct rt_mutex *m)
{
    if (!owner || !m)
    {
        return;
    }

    if (!m->owner_node.next || !m->owner_node.prev)
    {
        INIT_LIST_HEAD(&m->owner_node);
    }

    if (list_empty(&m->owner_node))
    {
        list_add(&m->owner_node, &owner->held_rt_mutexs);
    }
}

static void ttosSchedOnPrioChange(TASK_ID t)
{
    if (!t)
    {
        return;
    }

    /*
     * rt_mutex waiters are not on TTOS task queues, so only requeue in the
     * rt_mutex waiter rb-tree and continue PI propagation.
     */
    if (t->blocked_on)
    {
        rt_mutex_waiter_prio_changed(t);
        return;
    }

    /*
     * taskChangePriority touches ready queues/bitmaps. Hold kernel scheduling
     * lock on this PI path to avoid concurrent queue corruption.
     */
    ttosDisableTaskDispatchWithLock();
    taskChangePriority(t, t->taskCurPriority, FALSE);
    ttosEnableTaskDispatchWithLock();
}

static int waiter_cmp(struct rt_mutex_waiter *a, struct rt_mutex_waiter *b)
{
    if (a->task->taskCurPriority != b->task->taskCurPriority)
    {
        return a->task->taskCurPriority > b->task->taskCurPriority;
    }

    return a->task < b->task; // 用地址
}

static void waiter_refresh_top_locked(struct rt_mutex *m)
{
    struct rb_node *last = rb_last(&m->waiters);
    if (last)
    {
        m->top_waiter = container_of(last, struct rt_mutex_waiter, rb);
    }
    else
    {
        m->top_waiter = NULL;
    }
}

static void waiter_insert_locked(struct rt_mutex *m, struct rt_mutex_waiter *w)
{
    struct rb_root *root;
    struct rb_node **new, *parent;

    root = &m->waiters;
    new = &(root->rb_node);
    parent = NULL;

    if (unlikely(!RB_EMPTY_NODE(&w->rb)))
    {
        BUG();
    }

    /* Figure out where to put new node */
    while (*new)
    {
        struct rt_mutex_waiter *this = rb_entry(*new, struct rt_mutex_waiter, rb);

        parent = *new;
        if (unlikely(w == this))
        {
            BUG();
        }
        if (waiter_cmp(w, this))
        {
            new = &((*new)->rb_left);
        }
        else
        {
            new = &((*new)->rb_right);
        }
    }

    /* Add new node and rebalance tree. */
    rb_link_node(&w->rb, parent, new);
    rb_insert_color(&w->rb, root);

    if (!m->top_waiter || waiter_cmp(m->top_waiter, w))
    {
        m->top_waiter = w;
    }

    w->prio_snapshot = w->task->taskCurPriority;
}

static void waiter_remove_locked(struct rt_mutex *m, struct rt_mutex_waiter *w)
{
    if (RB_EMPTY_NODE(&w->rb))
    {
        return;
    }

    bool was_top = (m->top_waiter == w);
    rb_erase(&w->rb, &m->waiters);
    RB_CLEAR_NODE(&w->rb);

    if (was_top)
    {
        waiter_refresh_top_locked(m);
    }
}

/**
 * @brief 当等待者的优先级发生变化时应调用该接口处理优先级
 *
 * @param m 所在的pi锁
 * @param w pi锁等待实例
 */
void waiter_requeue_if_needed(struct rt_mutex *m, struct rt_mutex_waiter *w)
{
    unsigned long flags;

    if (!m || !w)
    {
        return;
    }

    spin_lock_irqsave(&m->waiters_lock, flags);
    if (w->task->blocked_on != m || RB_EMPTY_NODE(&w->rb))
    {
        spin_unlock_irqrestore(&m->waiters_lock, flags);
        return;
    }

    if (w->prio_snapshot != w->task->taskCurPriority)
    {
        waiter_remove_locked(m, w);
        waiter_insert_locked(m, w);
    }
    spin_unlock_irqrestore(&m->waiters_lock, flags);
}

/**
 * @brief 重新计算任务有效优先级
 *
 * @param t 任务
 */
static void recompute_eff_prio(TASK_ID t)
{
    if (!rt_mutex_task_alive(t))
    {
        return;
    }

    T_UBYTE new_prio = t->taskPriority;
    struct rt_mutex *m;
    unsigned long flags;
    spin_lock_irqsave(&t->held_rt_mutexs_lock, flags);

    if (!t->held_rt_mutexs.next || !t->held_rt_mutexs.prev)
    {
        INIT_LIST_HEAD(&t->held_rt_mutexs);
        spin_unlock_irqrestore(&t->held_rt_mutexs_lock, flags);
        return;
    }

    list_for_each_entry(m, &t->held_rt_mutexs, owner_node)
    {
        unsigned long wflags;

        spin_lock_irqsave(&m->waiters_lock, wflags);
        if (m->top_waiter)
        {
            new_prio = min(new_prio, m->top_waiter->task->taskCurPriority);
        }
        spin_unlock_irqrestore(&m->waiters_lock, wflags);
    }
    spin_unlock_irqrestore(&t->held_rt_mutexs_lock, flags);
    if (t->taskCurPriority != new_prio)
    {
        t->taskCurPriority = new_prio;
        ttosSchedOnPrioChange(t);
    }
}

/**
 * @brief 递归传播优先级
 *
 * @param t 任务
 */
static void propagate_prio(TASK_ID t)
{
    while (1)
    {
        struct rt_mutex *m;
        TASK_ID owner;
        unsigned long flags;

        m = READ_ONCE(t->blocked_on);
        if (!m)
        {
            break;
        }

        spin_lock_irqsave(&m->waiters_lock, flags);
        owner = m->owner;
        if (!rt_mutex_task_alive(owner) || owner->taskCurPriority <= t->taskCurPriority)
        {
            if (owner && !rt_mutex_task_alive(owner))
            {
                m->owner = NULL;
            }
            spin_unlock_irqrestore(&m->waiters_lock, flags);
            break;
        }

        owner->taskCurPriority = t->taskCurPriority;
        spin_unlock_irqrestore(&m->waiters_lock, flags);
        ttosSchedOnPrioChange(owner);

        t = owner;
    }
}

void rt_mutex_waiter_prio_changed(TASK_ID t)
{
    struct rt_mutex *m;

    if (!t)
    {
        return;
    }

    m = READ_ONCE(t->blocked_on);
    if (!m)
    {
        return;
    }

    waiter_requeue_if_needed(m, &t->waiter);
    propagate_prio(t);
}

void rt_mutex_wake_owner(struct rt_mutex *m, TASK_ID next)
{
    unsigned long flags;

    if (!m || !next)
    {
        return;
    }

    spin_lock_irqsave(&next->held_rt_mutexs_lock, flags);
    rt_mutex_owner_list_add_safe(next, m);
    spin_unlock_irqrestore(&next->held_rt_mutexs_lock, flags);

    /* If timeout-waited, remove it from tick waited queue before READY. */
    if (next->objCore.objectNode.next != NULL)
    {
        ttosExactWaitedTask(next);
    }

    next->state &= ~(TTOS_TASK_INTERRUPTIBLE_BY_SIGNAL | TTOS_TASK_WAITING_FOR_MUTEX);
    ttosClearTaskWaiting(next);
}

int rt_mutex_take_top_waiter(struct rt_mutex *m, TASK_ID *next)
{
    unsigned long flags;
    TASK_ID nxt = NULL;

    if (!m || !next)
    {
        return -EINVAL;
    }

    spin_lock_irqsave(&m->waiters_lock, flags);
    if (m->owner == NULL && m->top_waiter)
    {
        struct rt_mutex_waiter *w = m->top_waiter;
        nxt = w->task;
        waiter_remove_locked(m, w);
        nxt->blocked_on = NULL;
        m->owner = nxt;
    }
    spin_unlock_irqrestore(&m->waiters_lock, flags);

    *next = nxt;
    return 0;
}

int rt_mutex_init(struct rt_mutex *m)
{
    if (!m)
    {
        return -EINVAL;
    }
    memset(m, 0, sizeof(*m));

    m->waiters = RB_TREE_ROOT;
    spin_lock_init(&m->waiters_lock);
    INIT_LIST_HEAD(&m->owner_node);
    m->owner = NULL;
    m->top_waiter = NULL;

    return 0;
}

int rt_mutex_destroy(struct rt_mutex *m)
{
    unsigned long flags;

    if (!m)
    {
        return -EINVAL;
    }

    if (TRUE == ttosIsISR())
    {
        return (TTOS_CALLED_FROM_ISR);
    }

    spin_lock_irqsave(&m->waiters_lock, flags);
    if (m->owner || m->top_waiter || !RB_EMPTY_ROOT(&m->waiters))
    {
        spin_unlock_irqrestore(&m->waiters_lock, flags);
        return -EBUSY;
    }

    m->owner = NULL;
    m->top_waiter = NULL;
    m->waiters = RB_TREE_ROOT;
    INIT_LIST_HEAD(&m->owner_node);

    spin_unlock_irqrestore(&m->waiters_lock, flags);

    kfree(m);

    return 0;
}

struct rt_mutex *rt_mutex_create(void)
{
    int ret;
    struct rt_mutex *m = kmalloc(sizeof(struct rt_mutex), GFP_KERNEL);
    if (!m)
    {
        return m;
    }

    ret = rt_mutex_init(m);
    if (ret)
    {
        kfree(m);
        return NULL;
    }
    return m;
}

static int task_waiter_sanitize(TASK_ID t, struct rt_mutex *lock_held)
{
    struct rt_mutex *old;
    unsigned long flags;

    if (!t)
    {
        return -EINVAL;
    }

    old = READ_ONCE(t->blocked_on);
    if (RB_EMPTY_NODE(&t->waiter.rb) && old == NULL)
    {
        return 0;
    }

    /*
     * If waiter node is still linked from a previous wait path, detaching it
     * here prevents memset() from corrupting an existing rb-tree.
     */
    if (old == NULL)
    {
        RB_CLEAR_NODE(&t->waiter.rb);
        return 0;
    }

    if (!kernel_access_check(old, sizeof(*old), UACCESS_RW))
    {
        t->blocked_on = NULL;
        RB_CLEAR_NODE(&t->waiter.rb);
        return -EFAULT;
    }

    if (old == lock_held)
    {
        if (!RB_EMPTY_NODE(&t->waiter.rb))
        {
            waiter_remove_locked(old, &t->waiter);
        }
        t->blocked_on = NULL;
        return 0;
    }

    spin_lock_irqsave(&old->waiters_lock, flags);
    if (t->blocked_on == old)
    {
        if (!RB_EMPTY_NODE(&t->waiter.rb))
        {
            waiter_remove_locked(old, &t->waiter);
        }
        t->blocked_on = NULL;
    }
    spin_unlock_irqrestore(&old->waiters_lock, flags);

    return 0;
}

static int task_waiter_init(TASK_ID t, struct rt_mutex *lock_held, struct rt_mutex_waiter *w)
{
    int ret;

    if (!w)
    {
        return -EINVAL;
    }

    ret = task_waiter_sanitize(t, lock_held);
    if (ret)
    {
        printk("waiter sanitize failed: task=%p blocked_on=%p ret=%d\n", t,
               t ? t->blocked_on : NULL, ret);
        if (ret != -EFAULT)
        {
            return ret;
        }
    }

    memset(w, 0, sizeof(*w));
    RB_CLEAR_NODE(&w->rb);
    w->task = t;
    return 0;
}

int rt_mutex_lock_timeout(struct rt_mutex *m, unsigned int ticks)
{
    unsigned long flags;
    TASK_ID owner;
    TASK_ID boost_owner = NULL;

    /* @REPLACE_BRACKET: 在虚拟中断处理程序中执行此接口 */
    if (TRUE == ttosIsISR())
    {
        /* @REPLACE_BRACKET: TTOS_CALLED_FROM_ISR */
        return (TTOS_CALLED_FROM_ISR);
    }

    TTOS_DisablePreempt();

    spin_lock_irqsave(&m->waiters_lock, flags);
    owner = m->owner;
    if (owner && !rt_mutex_task_alive(owner))
    {
        owner = NULL;
        m->owner = NULL;
    }
    if (owner == NULL)
    {
        /* 快路径 */
        m->owner = TTOS_GetRunningTask();
        spin_unlock_irqrestore(&m->waiters_lock, flags);

        spin_lock_irqsave(&TTOS_GetRunningTask()->held_rt_mutexs_lock, flags);
        rt_mutex_owner_list_add_safe(TTOS_GetRunningTask(), m);
        spin_unlock_irqrestore(&TTOS_GetRunningTask()->held_rt_mutexs_lock, flags);
        TTOS_EnablePreempt();
        return 0;
    }

    if (owner == TTOS_GetRunningTask())
    {
        spin_unlock_irqrestore(&m->waiters_lock, flags);
        TTOS_EnablePreempt();
        return -EDEADLK;
    }

    if (ticks == 0)
    {
        spin_unlock_irqrestore(&m->waiters_lock, flags);
        TTOS_EnablePreempt();
        return TTOS_TIMEOUT;
    }

    /* 慢路径：阻塞 */
    if (task_waiter_init(TTOS_GetRunningTask(), m, &TTOS_GetRunningTask()->waiter))
    {
        spin_unlock_irqrestore(&m->waiters_lock, flags);
        TTOS_EnablePreempt();
        return -EINVAL;
    }
    TTOS_GetRunningTask()->blocked_on = m;

    waiter_insert_locked(m, &TTOS_GetRunningTask()->waiter);

    /* Priority Inheritance */
    if (owner && TTOS_GetRunningTask()->taskCurPriority < owner->taskCurPriority)
    {
        owner->taskCurPriority = TTOS_GetRunningTask()->taskCurPriority;
        boost_owner = owner;
    }

    TTOS_GetRunningTask()->wait.returnCode = 0;

    /* @KEEP_COMMENT: 禁止调度器 */
    ttosDisableTaskDispatchWithLock();

    (void)ttosClearTaskReady(TTOS_GetRunningTask());
    TTOS_GetRunningTask()->state = TTOS_TASK_INTERRUPTIBLE_BY_SIGNAL | TTOS_TASK_WAITING_FOR_MUTEX;
    spin_unlock_irqrestore(&m->waiters_lock, flags);

    if (ticks != TTOS_WAIT_FOREVER)
    {
        TTOS_GetRunningTask()->waitedTicks = ticks;
        (void)ttosInsertWaitedTask(TTOS_GetRunningTask());
    }

    /* @KEEP_COMMENT: 重新使能调度 */
    ttosEnableTaskDispatchWithLock();

    if (boost_owner)
    {
        ttosSchedOnPrioChange(boost_owner);
        propagate_prio(boost_owner);
    }

    TTOS_EnablePreempt();

    if (TTOS_GetRunningTask()->wait.returnCode)
    {
        rt_mutex_waiter_abort(TTOS_GetRunningTask());
    }

    /* 被唤醒后，已成为 owner */
    return TTOS_GetRunningTask()->wait.returnCode;
}

int rt_mutex_lock(struct rt_mutex *m)
{
    return rt_mutex_lock_timeout(m, TTOS_WAIT_FOREVER);
}

int rt_mutex_unlock_prepare(struct rt_mutex *m, TASK_ID *next)
{
    unsigned long flags;
    TASK_ID owner;
    TASK_ID nxt = NULL;

    if (!m || !next)
    {
        return -EINVAL;
    }

    TTOS_DisablePreempt();

    spin_lock_irqsave(&m->waiters_lock, flags);
    owner = m->owner;
    if (owner && !rt_mutex_task_alive(owner))
    {
        owner = NULL;
        m->owner = NULL;
    }
    if (owner != TTOS_GetRunningTask())
    {
        spin_unlock_irqrestore(&m->waiters_lock, flags);
        TTOS_EnablePreempt();
        return -EPERM;
    }

    m->owner = NULL;

    if (m->top_waiter)
    {
        struct rt_mutex_waiter *w = m->top_waiter;
        nxt = w->task;

        waiter_remove_locked(m, w);
        nxt->blocked_on = NULL;
        m->owner = nxt;
    }
    spin_unlock_irqrestore(&m->waiters_lock, flags);

    /* 移除持锁关系 */
    spin_lock_irqsave(&owner->held_rt_mutexs_lock, flags);
    list_del_init(&m->owner_node);
    spin_unlock_irqrestore(&owner->held_rt_mutexs_lock, flags);

    /* 精确撤销 PI */
    recompute_eff_prio(owner);

    TTOS_EnablePreempt();

    *next = nxt;
    return 0;
}

void rt_mutex_unlock_finish(struct rt_mutex *m, TASK_ID next)
{
    if (next)
    {
        rt_mutex_wake_owner(m, next);
    }
}

int rt_mutex_unlock(struct rt_mutex *m)
{
    TASK_ID next = NULL;
    int ret = rt_mutex_unlock_prepare(m, &next);
    if (ret)
    {
        return ret;
    }
    rt_mutex_unlock_finish(m, next);
    return 0;
}

int rt_mutex_enqueue_waiter(struct rt_mutex *m, TASK_ID t)
{
    unsigned long flags;
    TASK_ID owner;
    TASK_ID boost_owner = NULL;

    if (!m || !t)
    {
        return -EINVAL;
    }

    TTOS_DisablePreempt();

    spin_lock_irqsave(&m->waiters_lock, flags);
    owner = m->owner;

    if (task_waiter_init(t, m, &t->waiter))
    {
        spin_unlock_irqrestore(&m->waiters_lock, flags);
        TTOS_EnablePreempt();
        return -EINVAL;
    }
    t->blocked_on = m;

    waiter_insert_locked(m, &t->waiter);

    /* Priority Inheritance */
    if (owner && t->taskCurPriority < owner->taskCurPriority)
    {
        owner->taskCurPriority = t->taskCurPriority;
        boost_owner = owner;
    }
    spin_unlock_irqrestore(&m->waiters_lock, flags);

    if (boost_owner)
    {
        ttosSchedOnPrioChange(boost_owner);
        propagate_prio(boost_owner);
    }

    TTOS_EnablePreempt();
    return 0;
}

void rt_mutex_waiter_abort(TASK_ID t)
{
    TTOS_DisablePreempt();

    struct rt_mutex *m = t->blocked_on;
    TASK_ID owner;
    unsigned long flags;

    if (!m)
    {
        TTOS_EnablePreempt();
        return;
    }

    spin_lock_irqsave(&m->waiters_lock, flags);
    if (t->blocked_on != m)
    {
        spin_unlock_irqrestore(&m->waiters_lock, flags);
        TTOS_EnablePreempt();
        return;
    }
    owner = m->owner;
    waiter_remove_locked(m, &t->waiter);
    t->blocked_on = NULL;
    spin_unlock_irqrestore(&m->waiters_lock, flags);

    if (rt_mutex_task_alive(owner))
    {
        recompute_eff_prio(owner);
    }

    TTOS_EnablePreempt();
}
