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

#ifndef __KLIST_H
#define __KLIST_H

#include <linux/kref.h>
#include <linux/list.h>
#include <linux/spinlock.h>

struct klist_node;
struct klist
{
    spinlock_t k_lock;
    struct list_head k_list;
    void (*get)(struct klist_node *);
    void (*put)(struct klist_node *);
} __attribute__((aligned(sizeof(void *))));

#define KLIST_INIT(_name, _get, _put)                                                              \
    {                                                                                              \
        .k_lock = __SPIN_LOCK_UNLOCKED(_name.k_lock), .k_list = LIST_HEAD_INIT(_name.k_list),      \
        .get = _get, .put = _put,                                                                  \
    }

#define DEFINE_KLIST(_name, _get, _put) struct klist _name = KLIST_INIT(_name, _get, _put)

extern void klist_init(struct klist *k, void (*get)(struct klist_node *),
                       void (*put)(struct klist_node *));

struct klist_node
{
    void *n_klist; /* owned by klist implementation */
    struct list_head n_node;
    struct kref n_ref;
    unsigned int n_dead : 1;
    unsigned int n_visible : 1;
};

extern void klist_add_tail(struct klist_node *n, struct klist *k);
extern void klist_add_head(struct klist_node *n, struct klist *k);
extern void klist_add_behind(struct klist_node *n, struct klist_node *pos);
extern void klist_add_before(struct klist_node *n, struct klist_node *pos);

extern void klist_del(struct klist_node *n);
extern void klist_remove(struct klist_node *n);

extern int klist_node_attached(struct klist_node *n);

struct klist_iter
{
    struct klist *i_klist;
    struct klist_node *i_cur;
};

extern void klist_iter_init(struct klist *k, struct klist_iter *i);
extern void klist_iter_init_node(struct klist *k, struct klist_iter *i, struct klist_node *n);
extern void klist_iter_exit(struct klist_iter *i);
extern struct klist_node *klist_prev(struct klist_iter *i);
extern struct klist_node *klist_next(struct klist_iter *i);

#endif
