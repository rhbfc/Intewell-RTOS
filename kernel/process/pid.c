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
#include <list.h>
#include <pid.h>
#include <spinlock.h>
#include <tasklist_lock.h>
#include <ttosProcess.h>

static pid_set_t pid_pool;

struct pid_node
{
    pid_t pid;
    pcb_t leader;
    struct rb_node rb_node;
};

/* PID 从1 开始 */
static pid_t g_last_pid = 1;
static struct rb_root process_rb_root = RB_TREE_ROOT;
DEFINE_SPINLOCK(process_pid_lock);

static pid_t pid_alloc(void)
{
    pid_t pid;
    long flags;

    spin_lock_irqsave(&process_pid_lock, flags);
    while (PID_ISSET(g_last_pid, &pid_pool))
    {
        g_last_pid++;
        if (g_last_pid == CONFIG_PROCESS_MAX)
        {
            g_last_pid = 2;
        }
    };
    PID_SET(g_last_pid, &pid_pool);
    pid = g_last_pid++;
    if (g_last_pid == CONFIG_PROCESS_MAX)
    {
        g_last_pid = 2;
    }
    spin_unlock_irqrestore(&process_pid_lock, flags);

    return pid;
}

static void pid_free(pid_t pid)
{
    long flags;

    spin_lock_irqsave(&process_pid_lock, flags);
    PID_CLR(pid, &pid_pool);
    spin_unlock_irqrestore(&process_pid_lock, flags);
}

void process_foreach(void (*func)(pcb_t, void *), void *arg)
{
    struct rb_node *node;
    for (node = rb_first(&process_rb_root); node != NULL; node = rb_next(node))
    {
        func(rb_entry(node, struct pid_node, rb_node)->leader, arg);
    }
}

static pcb_t process_rb_search(pid_t pid)
{
    tasklist_lock();
    struct rb_node *node = process_rb_root.rb_node;
    while (node)
    {
        struct pid_node *this = rb_entry(node, struct pid_node, rb_node);

        if (pid < this->pid)
            node = node->rb_left;
        else if (pid > this->pid)
            node = node->rb_right;
        else
        {
            tasklist_unlock();
            return this->leader;
        }
    }

    tasklist_unlock();
    return NULL;
}
__weak_alias(process_rb_search, pcb_get_by_pid);

pcb_t pcb_get_by_pid_nt(pid_t pid)
{
    pcb_t pcb = process_rb_search(pid);
    if (pcb == NULL || pcb->group_exit_status.is_terminated)
    {
        return NULL;
    }
    return pcb;
}

static int process_rb_insert(struct pid_node *node)
{
    tasklist_lock();
    struct rb_node **new = &(process_rb_root.rb_node), *parent = NULL;
    /* Figure out where to put new node */
    while (*new)
    {
        struct pid_node *this = rb_entry(*new, struct pid_node, rb_node);

        parent = *new;
        if (node->pid < this->pid)
            new = &((*new)->rb_left);
        else if (node->pid > this->pid)
            new = &((*new)->rb_right);
        else
        {
            tasklist_unlock();
            return -1;
        }
    }

    /* Add new node and rebalance tree. */
    rb_link_node(&node->rb_node, parent, new);
    rb_insert_color(&node->rb_node, &process_rb_root);
    tasklist_unlock();
    return 0;
}

static void process_rb_earse(struct pid_node *pid_node)
{
    long flags;
    tasklist_lock();
    rb_erase(&pid_node->rb_node, &process_rb_root);
    tasklist_unlock();
    pid_free(pid_node->pid);
    free(pid_node);
}

pid_t pid_obj_get_pid(struct process_obj *obj)
{
    struct pid_node *pid_node = PROCESS_OBJ_GET(obj, struct pid_node *);
    return pid_node->pid;
}

void pcb_set_pid_leader(pcb_t pcb)
{
    struct pid_node *pid_node = PROCESS_OBJ_GET(pcb->pid, struct pid_node *);
    pid_node->leader = pcb;
}

struct process_obj *pid_obj_alloc(pcb_t pcb)
{
    struct pid_node *pid_node = malloc(sizeof(struct pid_node));

    if (pid_node == NULL)
    {
        return NULL;
    }
    pid_node->pid = pid_alloc();
    if (pid_node->pid < 0)
    {
        free(pid_node);
        return NULL;
    }
    pid_node->leader = pcb;
    pcb->pid = process_obj_create(pcb, (void *)pid_node, (void (*)(void *))process_rb_earse);

    if (pcb->pid == NULL)
    {
        free(pid_node);
        return NULL;
    }
    process_rb_insert(pid_node);
    return pcb->pid;
}

int pid_obj_ref(pcb_t parent, pcb_t child)
{
    child->pid = process_obj_ref(child, parent->pid);
    if (child->pid == NULL)
    {
        return -ENOMEM;
    }
    return 0;
}
