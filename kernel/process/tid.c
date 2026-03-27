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

#include <list.h>
#include <spinlock.h>
#include <tasklist_lock.h>
#include <ttos.h>
#include <ttosProcess.h>
#include <ttosRBTree.h>

struct tid_data
{
    struct list_head list;
    pid_t tid;
};

struct tid_node
{
    pid_t tid;
    TASK_ID taskControlId;
    struct rb_node rb_node;
};

static DEFINE_SPINLOCK(g_tid_lock);
static LIST_HEAD(g_free_tid_list_head);

/* PID 从1 开始 */
static pid_t last_tid = CONFIG_PROCESS_MAX;

static pid_t tid_alloc(void)
{
    struct tid_data *pdata;
    pid_t tid;
    long flags;
    spin_lock_irqsave(&g_tid_lock, flags);
    if (list_empty(&g_free_tid_list_head))
    {
        spin_unlock_irqrestore(&g_tid_lock, flags);
        return last_tid++;
    }
    pdata = list_entry(list_first(&g_free_tid_list_head), struct tid_data, list);
    tid = pdata->tid;
    list_del(&pdata->list);
    spin_unlock_irqrestore(&g_tid_lock, flags);
    free(pdata);
    return tid;
}

static void tid_free(pid_t tid)
{
    if (tid < CONFIG_PROCESS_MAX) // not need free
    {
        return;
    }
    struct tid_data *pdata = malloc(sizeof(struct tid_data));
    long flags;
    spin_lock_irqsave(&g_tid_lock, flags);
    pdata->tid = tid;
    list_add(&pdata->list, &g_free_tid_list_head);
    spin_unlock_irqrestore(&g_tid_lock, flags);
}

pid_t get_max_tid(void)
{
    return last_tid;
}

static struct rb_root task_rb_root = RB_TREE_ROOT;
DEFINE_SPINLOCK(task_rb_lock);

void task_foreach(void (*func)(TASK_ID, void *), void *arg)
{
    struct rb_node *node;
    for (node = rb_first(&task_rb_root); node != NULL; node = rb_next(node))
    {
        func(rb_entry(node, struct tid_node, rb_node)->taskControlId, arg);
    }
}

static TASK_ID task_rb_search(pid_t tid)
{
    long flags;
    tasklist_lock();
    struct rb_node *node = task_rb_root.rb_node;
    while (node)
    {
        struct tid_node *this = rb_entry(node, struct tid_node, rb_node);

        if (tid < this->tid)
            node = node->rb_left;
        else if (tid > this->tid)
            node = node->rb_right;
        else
        {
            tasklist_unlock();
            return this->taskControlId;
        }
    }
    tasklist_unlock();
    return NULL;
}
__weak_alias(task_rb_search, task_get_by_tid);

static int task_rb_insert(struct tid_node *node)
{
    long flags;
    tasklist_lock();
    struct rb_node **new = &(task_rb_root.rb_node), *parent = NULL;
    /* Figure out where to put new node */
    while (*new)
    {
        struct tid_node *this = rb_entry(*new, struct tid_node, rb_node);

        parent = *new;
        if (node->tid < this->tid)
            new = &((*new)->rb_left);
        else if (node->tid > this->tid)
            new = &((*new)->rb_right);
        else
        {
            tasklist_unlock();
            return -1;
        }
    }

    /* Add new node and rebalance tree. */
    rb_link_node(&node->rb_node, parent, new);
    rb_insert_color(&node->rb_node, &task_rb_root);
    tasklist_unlock();
    return 0;
}

static void task_rb_earse(struct tid_node *tid_node)
{
    long flags;
    tasklist_lock();
    rb_erase(&tid_node->rb_node, &task_rb_root);
    tasklist_unlock();
    tid_free(tid_node->tid);
    free(tid_node);
}

pid_t tid_obj_alloc(TASK_ID tcb)
{
    struct tid_node *tid_node = malloc(sizeof(struct tid_node));
    tid_node->tid = tid_alloc();
    tid_node->taskControlId = tcb;
    tcb->tid = tid_node->tid;
    task_rb_insert(tid_node);
    trace_object(tcb->ppcb ? TRACE_OBJ_THREAD : TRACE_OBJ_KTHREAD, tcb);
    return tcb->tid;
}

pid_t tid_obj_alloc_with_id(TASK_ID tcb, pid_t tid)
{
    struct tid_node *tid_node = malloc(sizeof(struct tid_node));
    tid_node->tid = tid;
    tid_node->taskControlId = tcb;
    tcb->tid = tid_node->tid;
    task_rb_insert(tid_node);
    return tcb->tid;
}

void tid_obj_free(pid_t tid)
{
    long flags;
    tasklist_lock();
    struct rb_node *node = task_rb_root.rb_node;
    while (node)
    {
        struct tid_node *this = rb_entry(node, struct tid_node, rb_node);

        if (tid < this->tid)
            node = node->rb_left;
        else if (tid > this->tid)
            node = node->rb_right;
        else
        {
            tasklist_unlock();
            task_rb_earse(this);
            return;
        }
    }
    tasklist_unlock();
}
