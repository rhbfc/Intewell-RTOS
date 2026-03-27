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

/*
 * @file： list.h
 * @brief：
 *      <li>双向链表相关接口实现</li>
 */
#ifndef _LIST_H
#define _LIST_H

/************************头文件********************************/
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <system/macros.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/************************宏定义********************************/
struct list_node;

/* 定义链表头，并兼容Linx链表结构 */
#define list_head list_node
#define list_add list_add_tail
#define list_empty list_is_empty
#define list_del list_delete

/* 初始化链表 */
#define LIST_HEAD_INIT(name)                                                                       \
    (struct list_node)                                                                             \
    {                                                                                              \
        &(name), &(name)                                                                           \
    }

/* 初始化链表 */
#define LIST_HEAD(name) struct list_head name = LIST_HEAD_INIT(name)

/* 链表迭代 */
#define list_for_each(node, head)                                                                  \
    for (node = (head)->next; !list_is_head(node, (head)); node = node->next)

/* 获取节点对应的结构体 */
#define list_entry(node, type, member) container_of(node, type, member)

/**
 * list_for_each_entry_reverse - iterate backwards over list of given type.
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 */
#define list_last_entry(ptr, type, member) list_entry((ptr)->prev, type, member)

/**
 * @brief list_first_entry - get the first element in list of given type
 * @param ptr:    the list head to take the element from.
 * @param type:   the type of the struct this is embedded in.
 * @param member: the name of the list_head within the struct.
 */
#define list_first_entry(ptr, type, member) list_entry((ptr)->next, type, member)

/**
 * list_next_entry - get the next element in list
 * @pos:	the type * to cursor
 * @member:	the name of the list_head within the struct.
 */
#define list_next_entry(pos, member) list_entry((pos)->member.next, typeof(*(pos)), member)

/**
 * list_prev_entry - get the prev element in list
 * @pos:    the type * to cursor
 * @member: the name of the list_head within the struct.
 */
#define list_prev_entry(pos, member) list_entry((pos)->member.prev, typeof(*(pos)), member)

/**
 * list_entry_is_head - test if the entry points to the head of the list
 * @pos:	the type * to cursor
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 */
#define list_entry_is_head(pos, head, member) list_is_head(&pos->member, (head))

/* 链表迭代节点对应的结构体 */
#define list_for_each_entry(pos, head, member)                                                     \
    for (pos = list_entry((head)->next, typeof(*pos), member); &pos->member != (head);             \
         pos = list_entry(pos->member.next, typeof(*pos), member))

/* 链表迭代节点对应的结构体,支持遍历时删除节点*/
#define list_for_each_entry_safe(pos, n, head, member)                                             \
    for (pos = list_entry((head)->next, typeof(*pos), member),                                     \
        n = list_entry(pos->member.next, typeof(*pos), member);                                    \
         &pos->member != (head); pos = n, n = list_entry(n->member.next, typeof(*n), member))

/**
 * list_for_each_entry_reverse - iterate backwards over list of given type.
 * @pos:    the type * to use as a loop cursor.
 * @head:   the head for your list.
 * @member: the name of the list_head within the struct.
 */
#define list_for_each_entry_reverse(pos, head, member)                                             \
    for (pos = list_last_entry(head, typeof(*pos), member);                                        \
         !list_entry_is_head(pos, head, member); pos = list_prev_entry(pos, member))

/**
 * list_for_each_entry_safe_reverse - iterate backwards over list safe against removal
 * @pos:	the type * to use as a loop cursor.
 * @n:		another type * to use as temporary storage
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 *
 * Iterate backwards over list of given type, safe against removal
 * of list entry.
 */
#define list_for_each_entry_safe_reverse(pos, n, head, member)                                     \
    for (pos = list_last_entry(head, typeof(*pos), member), n = list_prev_entry(pos, member);      \
         !list_entry_is_head(pos, head, member); pos = n, n = list_prev_entry(n, member))

/**
 * list_for_each_entry_safe_from - iterate over list from current point safe against removal
 * @pos:	the type * to use as a loop cursor.
 * @n:		another type * to use as temporary storage
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 *
 * Iterate over list of given type from current point, safe against
 * removal of list entry.
 */
#define list_for_each_entry_safe_from(pos, n, head, member)                                        \
    for (n = list_next_entry(pos, member); !list_entry_is_head(pos, head, member);                 \
         pos = n, n = list_next_entry(n, member))

/************************类型定义******************************/

/* 链表定义 */
struct list_node
{
    struct list_node *next;
    struct list_node *prev;
};

/* 初始化双向链表 */
static inline void INIT_LIST_HEAD(struct list_head *head)
{
    head->next = head;
    head->prev = head;
}

/* 添加节点到双向链表尾部 */
static inline void list_add_tail(struct list_node *node, struct list_head *head)
{
    node->prev = head->prev;
    node->next = head;
    head->prev->next = node;
    head->prev = node;
}

static inline void __list_add(struct list_head *_new, struct list_head *prev,
                              struct list_head *next)
{
    _new->prev = prev;
    _new->next = next;
    prev->next = _new;
    next->prev = _new;
}

static inline void list_add_after(struct list_head *_new, struct list_head *head)
{
    __list_add(_new, head, head->next);
}

static inline void list_add_before(struct list_head *node, struct list_head *head)
{
    head->prev->next = node;
    node->prev = head->prev;

    head->prev = node;
    node->next = head;
}

/* 添加节点到双向链表头部 */
static inline void list_add_head(struct list_node *node, struct list_head *head)
{
    node->next = head->next;
    node->prev = head;
    head->next->prev = node;
    head->next = node;
}

/* 删除节点 */
static inline void list_delete(struct list_node *node)
{
    node->next->prev = node->prev;
    node->prev->next = node->next;
    node->prev = node->next = NULL;
}

/* 删除链表头节点 */
static inline struct list_node *list_delete_head(struct list_head *head)
{
    if (head->next != head)
    {
        struct list_node *node = head->next;
        list_delete(node);
        return node;
    }
    else
    {
        return NULL;
    }
}

/* 删除链表尾节点 */
static inline struct list_node *list_delete_tail(struct list_head *head)
{
    if (head->prev != head)
    {
        struct list_node *node = head->prev;
        list_delete(node);
        return node;
    }
    else
    {
        return NULL;
    }
}

/* 返回前一个节点 */
static inline struct list_node *list_prev(struct list_node *node, struct list_head *head)
{
    if (node->prev != head)
    {
        return node->prev;
    }
    else
    {
        return NULL;
    }
}

/* 返回下一个节点 */
static inline struct list_node *list_next(struct list_node *node, struct list_head *head)
{
    if (node->next != head)
    {
        return node->next;
    }
    else
    {
        return NULL;
    }
}

/* 返回链表的头节点 */
static inline struct list_node *list_first(struct list_head *head)
{
    if (head->next != head)
    {
        return head->next;
    }
    else
    {
        return NULL;
    }
}

/* 返回链表的尾节点 */
static inline struct list_node *list_tail(struct list_head *head)
{
    if (head->prev != head)
    {
        return head->prev;
    }
    else
    {
        return NULL;
    }
}

/* 判断双向链表是否为空 */
static inline bool list_is_empty(struct list_head *head)
{
    return (head->next == head) ? true : false;
}

/* 判断节点是否是第一个 */
static inline int list_is_first(const struct list_node *node, const struct list_head *head)
{
    return node->prev == head;
}

/* 判断节点是否是最后一个 */
static inline int list_is_last(const struct list_node *node, const struct list_head *head)
{
    return node->next == head;
}

/* 判断节点是否是链表头 */
static inline int list_is_head(const struct list_node *node, const struct list_head *head)
{
    return node == head;
}

/* 链表节点数量 */
static inline size_t list_count(struct list_head *head)
{
    struct list_node *node;
    size_t count = 0;

    list_for_each(node, head)
    {
        count++;
    }
    return count;
}

/**
 * list_replace - replace old entry by _new one
 * @old : the element to be replaced
 * @_new : the _new element to insert
 *
 * If @old was empty, it will be overwritten.
 */
static inline void list_replace(struct list_head *old, struct list_head *_new)
{
    _new->next = old->next;
    _new->next->prev = _new;
    _new->prev = old->prev;
    _new->prev->next = _new;
}

/**
 * list_del_init - deletes entry from list and reinitialize it.
 * @entry: the element to delete from the list.
 */
static inline void list_del_init(struct list_head *entry)
{
    list_delete(entry);
    INIT_LIST_HEAD(entry);
}

/************************接口声明******************************/

#ifdef __cplusplus
}
#endif /*__cplusplus */

#endif /*_LIST_H */
