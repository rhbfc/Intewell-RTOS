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

#ifndef __KTIMERQUEUE_H
#define __KTIMERQUEUE_H

#include <stdbool.h>
#include <ttosRBTree.h>
#include <time/ktime.h>

struct ktimerqueue_entry
{
	struct rb_node rb_link;
	ktime_t expires;
};

struct ktimerqueue
{
	struct rb_root tree;
	/* 缓存最早到期节点 */
	struct ktimerqueue_entry *earliest;
};

static inline struct ktimerqueue_entry *ktimerqueue_peek(const struct ktimerqueue *queue)
{
	return queue->earliest;
}

static inline bool ktimerqueue_is_empty(const struct ktimerqueue *queue)
{
	return queue->earliest == NULL;
}

static inline bool ktimerqueue_entry_is_linked(const struct ktimerqueue_entry *entry)
{
	return !RB_EMPTY_NODE(&entry->rb_link);
}

static inline void ktimerqueue_entry_init(struct ktimerqueue_entry *entry)
{
	rb_init_node(&entry->rb_link);
}

static inline void ktimerqueue_init(struct ktimerqueue *queue)
{
	queue->tree = RB_ROOT_INIT;
	queue->earliest = NULL;
}

bool ktimerqueue_add(struct ktimerqueue *queue, struct ktimerqueue_entry *entry);
void ktimerqueue_del(struct ktimerqueue *queue, struct ktimerqueue_entry *entry);
struct ktimerqueue_entry *ktimerqueue_next(struct ktimerqueue_entry *entry);
#endif /* __KTIMERQUEUE_H */
