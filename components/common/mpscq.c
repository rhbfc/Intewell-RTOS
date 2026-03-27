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

#include <commonUtils.h>
#include <mpscq.h>
#include <page.h>
#include <ttosMM.h>
#include <barrier.h>

#define MPSCQ_POW2_MAX_U32 0x80000000U
#if UINTPTR_MAX == 0xffffffffUL
#define MPSCQ_MAX_SLOTS_U32 0x40000000U
#else
#define MPSCQ_MAX_SLOTS_U32 MPSCQ_POW2_MAX_U32
#endif

/* 下一个2的幂 */
static inline uint32_t next_pow2_u32(uint32_t v)
{
    if (v == 0) return 1;
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

static int mpscq_bytes_to_slots(const mpscq_t *r, uint32_t bytes, uint32_t *n)
{
    uintptr_t slots;
    uintptr_t need;

    if (!r || !n || bytes == 0 || r->elem_stride == 0)
        return -1;

    need = (uintptr_t) bytes + (uintptr_t) r->elem_mask;
    if (need < (uintptr_t) bytes)
        return -1;

    slots = need >> (uint32_t) r->elem_bit;
    if (slots == 0 || slots > UINT32_MAX)
        return -1;

    *n = (uint32_t) slots;
    return 0;
}

static int mpscq_try_normalize_empty(mpscq_t *r, uintptr_t seq)
{
    uint32_t idx;
    uintptr_t base;

    if (!r || r->size == 0)
        return 0;

    idx = (uint32_t) seq & r->mask;
    if (idx == 0)
        return 0;

    if ((uintptr_t) atomic_read(&r->commit_tail) != seq)
        return 0;
    if ((uintptr_t) atomic_read(&r->reserve_tail) != seq)
        return 0;

    base = seq + (uintptr_t) (r->size - idx);
    if (base <= seq)
        return 0;

    if (!atomic_cas(&r->reserve_tail, (long) seq, (long) base))
        return 0;

    (void) atomic_cas(&r->commit_tail, (long) seq, (long) base);
    (void) atomic_cas(&r->head, (long) seq, (long) base);
    return 1;
}

/* 初始化 */
int mpscq_init(mpscq_t *r, size_t capacity, size_t elem_size)
{
    uint32_t sz;
    uint32_t stride;
    size_t bytes;

    if (!r || capacity == 0 || elem_size == 0)
        return -1;

    r->phyaddr = 0;
    r->data = NULL;

    if (capacity > MPSCQ_POW2_MAX_U32 || elem_size > MPSCQ_POW2_MAX_U32)
        return -1;

    sz = next_pow2_u32((uint32_t) capacity);
    stride = next_pow2_u32((uint32_t) elem_size);

    if (sz == 0 || stride == 0)
        return -1;
    if (sz > MPSCQ_MAX_SLOTS_U32)
        return -1;

    r->size = sz;
    r->mask = sz - 1;

    /* 真实元素大小用于 memcpy */
    r->elem_size = elem_size;

    /* stride 用于地址步进（pow2 对齐）。如果你不需要对齐，可直接 stride=elem_size */
    r->elem_stride = stride;
    r->elem_mask = r->elem_stride - 1;
    r->elem_bit = 31u - (uint32_t) __builtin_clz((uint32_t) r->elem_stride);

    if (r->elem_stride > 0 && (size_t) r->size > SIZE_MAX / r->elem_stride)
        return -1;

    bytes = r->elem_stride * (size_t) r->size;
    r->phyaddr = pages_alloc(page_bits(bytes), ZONE_NORMAL);
    r->data = (void *) page_address(r->phyaddr);
    if (!r->data)
        return -1;

    atomic_write(&r->head, 0);
    atomic_write(&r->reserve_tail, 0);
    atomic_write(&r->commit_tail, 0);
    return 0;
}

void mpscq_destroy(mpscq_t *r)
{
    if (!r) return;
    if (!r->phyaddr) return;

    size_t bytes = r->elem_stride * (size_t) r->size;
    pages_free(r->phyaddr, page_bits(bytes));
    r->phyaddr = 0;
    r->data = NULL;
}

/**
 * @brief
 *   返回当前队列中【已提交、尚未被消费】的元素个数
 *   适用于 MPSC（多生产者/单消费者）
 */
uint32_t mpscq_count(mpscq_t *r)
{
    if (!r)
        return 0;

    uintptr_t head = (uintptr_t) atomic_read(&r->head);
    uintptr_t tail = (uintptr_t) atomic_read(&r->commit_tail);

    return (uint32_t) (tail - head);
}

/* 内部：判断本次 n 个元素是否会跨 ring 末尾（idx+n>size） */
static inline int mpscq_cross_boundary(const mpscq_t *r, uint32_t idx, uint32_t n)
{
    return (idx + n > r->size);
}

/* reserve：多生产者抢占位置，返回连续指针，并输出 token */
void *mpscq_alloc(mpscq_t *r, uint32_t n, mpscq_token_t *tok)
{
    if (!r || !tok || n == 0 || n > r->size)
        return NULL;

    while (1)
    {
        uintptr_t head = (uintptr_t) atomic_read(&r->head);
        uintptr_t old = (uintptr_t) atomic_read(&r->reserve_tail);
        uintptr_t used = old - head;

        if (used + (uintptr_t) n > (uintptr_t) r->size)
            return NULL; /* 空间不足 */

        uint32_t idx = (uint32_t) old & r->mask;

        /* 最小策略：为了保证返回“连续内存”，禁止跨界 */
        if (mpscq_cross_boundary(r, idx, n)) {
            if (used == 0 && (uintptr_t) atomic_read(&r->commit_tail) == old)
            {
                if (mpscq_try_normalize_empty(r, old))
                    continue;
            }
            return NULL;
        }

        /* CAS 抢占 [old, old+n) */
        if (atomic_cas(&r->reserve_tail, (long) old, (long) (old + n)))
        {
            *tok = (mpscq_token_t) old;
            return (void *) ((char *) r->data + (size_t) idx * r->elem_stride);
        }
        /* CAS 失败继续抢 */
    }
}

void *mpscq_alloc_bytes(mpscq_t *r, uint32_t len, mpscq_token_t *tok)
{
    uint32_t n;
    if (!r || !tok) return NULL;
    if (mpscq_bytes_to_slots(r, len, &n) != 0)
        return NULL;
    return mpscq_alloc(r, n, tok);
}

/*
 * commit：按序发布
 * - 生产者写完数据后，必须等 commit_tail == tok 才能推进 commit_tail
 * - 推进前用 release fence，保证数据对消费者可见
 */
int mpscq_push(mpscq_t *r, mpscq_token_t tok, uint32_t n)
{
    uintptr_t ct;
    uintptr_t reserve;
    uintptr_t next;

    if (!r || n == 0 || n > r->size)
        return -1;

    if ((uintptr_t) tok > UINTPTR_MAX - (uintptr_t) n)
        return -1;

    next = (uintptr_t) tok + (uintptr_t) n;

    reserve = (uintptr_t) atomic_read(&r->reserve_tail);
    if (next > reserve)
        return -1;

    /* 等待轮到自己提交，保证 commit_tail 连续 */
    while ((ct = (uintptr_t) atomic_read(&r->commit_tail)) != (uintptr_t) tok) {
        if (ct > (uintptr_t) tok)
            return -1;
        /* 没有 cpu_relax/yield，只能空转 */
    }

    smp_wmb(); /* release：先保证数据写入可见，再发布 commit_tail */
    atomic_write(&r->commit_tail, (long) next);
    return 0;
}

int mpscq_push_bytes(mpscq_t *r, mpscq_token_t tok, uint32_t bytes)
{
    uint32_t n;
    if (mpscq_bytes_to_slots(r, bytes, &n) != 0)
        return -1;
    return mpscq_push(r, tok, n);
}

/* peek：单消费者，只看 commit_tail */
void *mpscq_peek(mpscq_t *r, uint32_t n)
{
    if (!r || n == 0)
        return NULL;

    uintptr_t head = (uintptr_t) atomic_read(&r->head);
    uintptr_t tail = (uintptr_t) atomic_read(&r->commit_tail);

    if (tail - head < (uintptr_t) n)
        return NULL;

    /* acquire：看到 commit_tail 后，确保随后读 data 不会乱序到前面 */
    smp_rmb();

    uint32_t idx = (uint32_t) head & r->mask;

    /* 同样禁止跨界读取：否则需要 split memcpy */
    if (mpscq_cross_boundary(r, idx, n))
        return NULL;

    return (void *) ((char *) r->data + (size_t) idx * r->elem_stride);
}

/* pop：单消费者推进 head */
void mpscq_pop(mpscq_t *r, uint32_t n)
{
    uintptr_t new_head;
    uintptr_t tail;

    if (!r || n == 0)
        return;

    new_head = (uintptr_t) atomic_add_return(&r->head, (long) n);
    tail = (uintptr_t) atomic_read(&r->commit_tail);
    if (new_head == tail)
        (void) mpscq_try_normalize_empty(r, new_head);
}

/* 一次性入队拷贝 n 个元素 */
int mpscq_enqueue(mpscq_t *r, const void *data, uint32_t n)
{
    uint32_t i;
    if (!r || !data || n == 0) return -1;

    mpscq_token_t tok;
    void *slot = mpscq_alloc(r, n, &tok);
    if (!slot)
        return -1;

    /* n>1 且 stride!=elem_size 时，必须按 slot 步进拷贝。 */
    for (i = 0; i < n; i++) {
        memcpy((char *) slot + (size_t) i * r->elem_stride, (const char *) data + (size_t) i * r->elem_size,
               r->elem_size);
    }

    /* 发布 */
    return mpscq_push(r, tok, n);
}

int mpscq_enqueue_bytes(mpscq_t *r, const void *data, uint32_t bytes)
{
    uint32_t n;
    if (!r || !data || bytes == 0) return -1;

    if (mpscq_bytes_to_slots(r, bytes, &n) != 0)
        return -1;

    mpscq_token_t tok;
    void *slot = mpscq_alloc(r, n, &tok);
    if (!slot)
        return -1;

    memcpy(slot, data, (size_t) bytes);
    return mpscq_push(r, tok, n);
}

/* 一次性出队拷贝 n 个元素 */
int mpscq_dequeue(mpscq_t *r, void *out, uint32_t n)
{
    uint32_t i;
    if (!r || !out || n == 0) return -1;

    void *slot = mpscq_peek(r, n);
    if (!slot)
        return -1;

    for (i = 0; i < n; i++) {
        memcpy((char *) out + (size_t) i * r->elem_size, (const char *) slot + (size_t) i * r->elem_stride,
               r->elem_size);
    }
    mpscq_pop(r, n);
    return 0;
}

int mpscq_dequeue_bytes(mpscq_t *r, void *out, uint32_t bytes)
{
    uint32_t n;
    if (!r || !out || bytes == 0) return -1;

    if (mpscq_bytes_to_slots(r, bytes, &n) != 0)
        return -1;

    void *slot = mpscq_peek(r, n);
    if (!slot)
        return -1;

    memcpy(out, slot, (size_t) bytes);
    mpscq_pop(r, n);
    return 0;
}
