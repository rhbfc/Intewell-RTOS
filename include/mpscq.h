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

#ifndef __MPSCQ_H__
#define __MPSCQ_H__

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <atomic.h>

/* reserve 时返回的 token：表示这次分配的起始序号（绝对位置） */
typedef uintptr_t mpscq_token_t;

/* 多生产者/单消费者队列 */
typedef struct mpscq {
    atomic_t reserve_tail;  /* producers: 抢占位置（reserve） */
    atomic_t commit_tail;   /* producers: 发布位置（commit），consumer 只看这个 */
    atomic_t head;          /* consumer: 已消费位置 */

    uint32_t size;          /* capacity (pow2) */
    uint32_t mask;

    size_t   elem_size;     /* 真实元素大小（memcpy 用） */
    size_t   elem_stride;   /* stride（pow2，对齐/步进用） */
    size_t   elem_mask;
    size_t   elem_bit;

    phys_addr_t phyaddr;
    void *data;
} mpscq_t;

int  mpscq_init(mpscq_t *r, size_t capacity, size_t elem_size);
void mpscq_destroy(mpscq_t *r);

uint32_t mpscq_count(mpscq_t *r);
/*
 * 多生产者申请连续 n 个 slot：
 * - 成功：返回起始指针，并输出 tok（必须用于 push/commit）
 * - 失败：返回 NULL
 *
 * 注意：本实现为了保证“返回连续内存”，禁止跨 ring 末尾分配。
 */
void *mpscq_alloc(mpscq_t *r, uint32_t n, mpscq_token_t *tok);
void *mpscq_alloc_bytes(mpscq_t *r, uint32_t len, mpscq_token_t *tok);

/*
 * 单消费者读取连续 n 个 slot（只读不 pop）
 * 同样禁止跨 ring 末尾读取（否则需要分段 memcpy）
 */
void *mpscq_peek(mpscq_t *r, uint32_t n);
void  mpscq_pop(mpscq_t *r, uint32_t n);

/*
 * 生产者发布（commit）。
 * - mpscq_push: n 的单位是 slot 数
 * - mpscq_push_bytes: bytes 的单位是字节数（内部会换算成 slot 数）
 * 返回 0 表示成功，-1 表示参数错误或发布失败。
 */
int mpscq_push(mpscq_t *r, mpscq_token_t tok, uint32_t n);
int mpscq_push_bytes(mpscq_t *r, mpscq_token_t tok, uint32_t bytes);

/* 一次性入队/出队（已经处理好 reserve/commit） */
int mpscq_enqueue(mpscq_t *r, const void *data, uint32_t n);
int mpscq_enqueue_bytes(mpscq_t *r, const void *data, uint32_t bytes);

int mpscq_dequeue(mpscq_t *r, void *out, uint32_t n);
int mpscq_dequeue_bytes(mpscq_t *r, void *out, uint32_t bytes);

#endif /* __MPSCQ_H__ */
