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

#include <stdbool.h>
#include <time/ktimerqueue.h>

#define ktimerqueue_entry_from_rb(_node) rb_entry((_node), struct ktimerqueue_entry, rb_link)

static inline bool ktimerqueue_entry_earlier(const struct ktimerqueue_entry *lhs,
                                             const struct ktimerqueue_entry *rhs)
{
    return lhs->expires < rhs->expires;
}

bool ktimerqueue_add(struct ktimerqueue *queue, struct ktimerqueue_entry *entry)
{
    struct rb_root *root = &queue->tree;
    struct rb_node **link = &root->rb_node;
    struct rb_node *parent = NULL;

    while (*link)
    {
        parent = *link;
        if (ktimerqueue_entry_earlier(entry, ktimerqueue_entry_from_rb(parent)))
        {
            link = &parent->rb_left;
        }
        else
        {
            link = &parent->rb_right;
        }
    }

    rb_link_node(&entry->rb_link, parent, link);
    rb_insert_color(&entry->rb_link, root);

    if (!queue->earliest || ktimerqueue_entry_earlier(entry, queue->earliest))
        queue->earliest = entry;

    return queue->earliest == entry;
}

void ktimerqueue_del(struct ktimerqueue *queue, struct ktimerqueue_entry *entry)
{
    struct ktimerqueue_entry *next;

    if (!ktimerqueue_entry_is_linked(entry))
        return;

    next = NULL;
    if (queue->earliest == entry)
        next = ktimerqueue_next(entry);

    rb_erase(&entry->rb_link, &queue->tree);
    rb_init_node(&entry->rb_link);

    if (queue->earliest == entry)
        queue->earliest = next;
}

struct ktimerqueue_entry *ktimerqueue_next(struct ktimerqueue_entry *entry)
{
    struct rb_node *next;

    if (!entry)
        return NULL;

    next = rb_next(&entry->rb_link);
    if (!next)
        return NULL;

    return ktimerqueue_entry_from_rb(next);
}
