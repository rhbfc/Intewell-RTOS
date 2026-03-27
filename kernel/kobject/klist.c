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

#include <barrier.h>
#include <linux/klist.h>
#include <linux/wait.h>
#include <system/bug.h>

struct klist_wait_token
{
    struct list_head link;
    struct klist_node *node;
    wait_queue_head_t waitq;
    int done;
};

static DEFINE_SPINLOCK(klist_wait_lock);
static LIST_HEAD(klist_wait_tokens);

static inline struct klist *klist_owner(const struct klist_node *node)
{
    return (struct klist *)node->n_klist;
}

static void klist_detach_node_locked(struct klist_node *node)
{
    struct klist_wait_token *token;
    struct klist_wait_token *next;

    if (node->n_visible)
    {
        list_del(&node->n_node);
        node->n_visible = 0;
    }

    node->n_klist = NULL;

    spin_lock(&klist_wait_lock);
    list_for_each_entry_safe(token, next, &klist_wait_tokens, link)
    {
        if (token->node != node)
            continue;

        list_del(&token->link);
        token->done = 1;
        mb();
        wake_up(&token->waitq);
    }
    spin_unlock(&klist_wait_lock);
}

static void klist_release_ref(struct kref *ref)
{
    struct klist_node *node = container_of(ref, struct klist_node, n_ref);
    klist_detach_node_locked(node);
}

static int klist_drop_ref_locked(struct klist_node *node)
{
    return kref_put(&node->n_ref, klist_release_ref);
}

static void klist_node_prepare(struct klist *list, struct klist_node *node)
{
    INIT_LIST_HEAD(&node->n_node);
    kref_init(&node->n_ref);
    node->n_klist = list;
    node->n_dead = 0;
    node->n_visible = 1;

    if (list->get)
        list->get(node);
}

static void klist_insert_head_locked(struct klist *list, struct klist_node *node)
{
    list_add_head(&node->n_node, &list->k_list);
}

static void klist_insert_tail_locked(struct klist *list, struct klist_node *node)
{
    list_add_tail(&node->n_node, &list->k_list);
}

static void klist_release_external(struct klist_node *node, void (*put)(struct klist_node *),
                                   int released)
{
    if (put && released)
        put(node);
}

static int klist_mark_dead_locked(struct klist_node *node)
{
    if (node->n_dead)
        return 0;

    node->n_dead = 1;
    return 1;
}

static void klist_drop_node(struct klist_node *node, bool remove_from_list)
{
    struct klist *list = klist_owner(node);
    void (*put)(struct klist_node *) = NULL;
    int dropped = 0;

    if (!list)
        return;

    put = list->put;

    spin_lock(&list->k_lock);

    if (remove_from_list && !klist_mark_dead_locked(node))
        put = NULL;

    if (!klist_drop_ref_locked(node))
        put = NULL;
    else
        dropped = 1;

    spin_unlock(&list->k_lock);

    klist_release_external(node, put, dropped);
}

void klist_init(struct klist *list, void (*get)(struct klist_node *),
                void (*put)(struct klist_node *))
{
    INIT_LIST_HEAD(&list->k_list);
    spin_lock_init(&list->k_lock);
    list->get = get;
    list->put = put;
}

void klist_add_head(struct klist_node *node, struct klist *list)
{
    klist_node_prepare(list, node);

    spin_lock(&list->k_lock);
    klist_insert_head_locked(list, node);
    spin_unlock(&list->k_lock);
}

void klist_add_tail(struct klist_node *node, struct klist *list)
{
    klist_node_prepare(list, node);

    spin_lock(&list->k_lock);
    klist_insert_tail_locked(list, node);
    spin_unlock(&list->k_lock);
}

void klist_add_behind(struct klist_node *node, struct klist_node *pos)
{
    struct klist *list = klist_owner(pos);

    WARN_ON(list == NULL);
    if (!list)
        return;

    klist_node_prepare(list, node);

    spin_lock(&list->k_lock);
    list_add_after(&node->n_node, &pos->n_node);
    spin_unlock(&list->k_lock);
}

void klist_add_before(struct klist_node *node, struct klist_node *pos)
{
    struct klist *list = klist_owner(pos);

    WARN_ON(list == NULL);
    if (!list)
        return;

    klist_node_prepare(list, node);

    spin_lock(&list->k_lock);
    list_add_before(&node->n_node, &pos->n_node);
    spin_unlock(&list->k_lock);
}

void klist_del(struct klist_node *node)
{
    klist_drop_node(node, true);
}

void klist_remove(struct klist_node *node)
{
    struct klist_wait_token token;

    if (!node)
        return;

    token.node = node;
    token.done = 0;
    init_waitqueue_head(&token.waitq);

    spin_lock(&klist_wait_lock);
    list_add_tail(&token.link, &klist_wait_tokens);
    spin_unlock(&klist_wait_lock);

    klist_del(node);

    if (!klist_node_attached(node))
    {
        spin_lock(&klist_wait_lock);
        if (token.link.next && token.link.prev)
            list_del(&token.link);
        spin_unlock(&klist_wait_lock);
        return;
    }

    wait_event(&token.waitq, token.done);
}

int klist_node_attached(struct klist_node *node)
{
    return node && node->n_klist != NULL;
}

void klist_iter_init_node(struct klist *list, struct klist_iter *iter, struct klist_node *node)
{
    iter->i_klist = list;
    iter->i_cur = NULL;

    if (!node)
        return;

    spin_lock(&list->k_lock);
    if (!node->n_dead && node->n_klist == list)
    {
        kref_get(&node->n_ref);
        iter->i_cur = node;
    }
    spin_unlock(&list->k_lock);
}

void klist_iter_init(struct klist *list, struct klist_iter *iter)
{
    klist_iter_init_node(list, iter, NULL);
}

void klist_iter_exit(struct klist_iter *iter)
{
    if (!iter->i_cur)
        return;

    klist_drop_node(iter->i_cur, false);
    iter->i_cur = NULL;
}

static struct klist_node *klist_node_from_link(struct list_head *link)
{
    return container_of(link, struct klist_node, n_node);
}

static struct klist_node *klist_step(struct klist_iter *iter, bool forward)
{
    struct klist *list = iter->i_klist;
    struct klist_node *current = iter->i_cur;
    struct klist_node *found = NULL;
    struct list_head *cursor;
    struct list_head *head = &list->k_list;
    void (*put)(struct klist_node *) = list->put;
    unsigned long flags;
    int dropped = 0;

    spin_lock_irqsave(&list->k_lock, flags);

    if (current)
    {
        cursor = forward ? current->n_node.next : current->n_node.prev;
        if (!klist_drop_ref_locked(current))
            put = NULL;
        else
            dropped = 1;
    }
    else
    {
        cursor = forward ? head->next : head->prev;
        put = NULL;
    }

    iter->i_cur = NULL;

    while (cursor != head)
    {
        struct klist_node *candidate = klist_node_from_link(cursor);

        cursor = forward ? cursor->next : cursor->prev;
        if (candidate->n_dead)
            continue;

        kref_get(&candidate->n_ref);
        iter->i_cur = candidate;
        found = candidate;
        break;
    }

    spin_unlock_irqrestore(&list->k_lock, flags);

    klist_release_external(current, put, dropped);
    return found;
}

struct klist_node *klist_prev(struct klist_iter *iter)
{
    return klist_step(iter, false);
}

struct klist_node *klist_next(struct klist_iter *iter)
{
    return klist_step(iter, true);
}
