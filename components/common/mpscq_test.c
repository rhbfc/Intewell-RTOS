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

#include <stdint.h>
#include <string.h>
#include <ttos.h>

#define MPSCQ_TEST_TAG "[mpscq_test] "
#define TEST_LOG(fmt, ...) printk(MPSCQ_TEST_TAG fmt "\n", ##__VA_ARGS__)

typedef int (*mpscq_issue_case_fn_t)(void);

struct mpscq_issue_case {
    const char *name;
    const char *desc;
    mpscq_issue_case_fn_t fn;
};

struct mpscq_case_error {
    const char *func;
    int line;
    const char *reason;
};

static struct mpscq_case_error g_last_case_error;

static inline void mpscq_case_error_reset(void)
{
    g_last_case_error.func = NULL;
    g_last_case_error.line = 0;
    g_last_case_error.reason = NULL;
}

#define CASE_FAIL(_why)                                                                        \
    do {                                                                                       \
        g_last_case_error.func = __func__;                                                     \
        g_last_case_error.line = __LINE__;                                                     \
        g_last_case_error.reason = (_why);                                                     \
        return -1;                                                                             \
    } while (0)

static int case_cross_boundary_stall(void)
{
    mpscq_t q;
    uint8_t in[7][8];
    uint8_t out[7][8];
    int reproduced = 1;
    int i;

    memset(&q, 0, sizeof(q));
    memset(in, 0x5a, sizeof(in));
    memset(out, 0, sizeof(out));
    mpscq_case_error_reset();

    if (mpscq_init(&q, 8, 8) != 0)
        CASE_FAIL("mpscq_init failed");

    if (mpscq_enqueue(&q, in, 7) != 0) {
        mpscq_destroy(&q);
        CASE_FAIL("mpscq_enqueue(7) failed");
    }

    if (mpscq_dequeue(&q, out, 7) != 0) {
        mpscq_destroy(&q);
        CASE_FAIL("mpscq_dequeue(7) failed");
    }

    if (mpscq_count(&q) != 0) {
        mpscq_destroy(&q);
        CASE_FAIL("mpscq_count != 0 after dequeue");
    }

    for (i = 0; i < 32; i++) {
        mpscq_token_t tok;
        void *p = mpscq_alloc(&q, 2, &tok);
        if (p != NULL) {
            reproduced = 0;
            mpscq_push(&q, tok, 2);
            mpscq_pop(&q, 2);
            break;
        }
    }

    {
        mpscq_token_t tok;
        void *p = mpscq_alloc(&q, 1, &tok);
        if (p == NULL) {
            mpscq_destroy(&q);
            CASE_FAIL("sanity mpscq_alloc(1) failed");
        }
        mpscq_push(&q, tok, 1);
        mpscq_pop(&q, 1);
    }

    mpscq_destroy(&q);
    return reproduced;
}

static int case_stride_mismatch_batch_copy(void)
{
    mpscq_t q;
    uint8_t src[2][24];
    uint8_t out0[24];
    uint8_t out1[24];
    int i;
    int reproduced = 0;

    memset(&q, 0, sizeof(q));
    memset(out0, 0, sizeof(out0));
    memset(out1, 0, sizeof(out1));
    mpscq_case_error_reset();

    for (i = 0; i < 24; i++) {
        src[0][i] = (uint8_t)i;
        src[1][i] = (uint8_t)(0x80 + i);
    }

    if (mpscq_init(&q, 8, 24) != 0)
        CASE_FAIL("mpscq_init failed");

    memset(q.data, 0, q.elem_stride * q.size);

    if (mpscq_enqueue(&q, src, 2) != 0) {
        mpscq_destroy(&q);
        CASE_FAIL("mpscq_enqueue(2) failed");
    }

    if (mpscq_dequeue(&q, out0, 1) != 0) {
        mpscq_destroy(&q);
        CASE_FAIL("mpscq_dequeue first slot failed");
    }

    if (mpscq_dequeue(&q, out1, 1) != 0) {
        mpscq_destroy(&q);
        CASE_FAIL("mpscq_dequeue second slot failed");
    }

    if (memcmp(out0, src[0], sizeof(out0)) != 0) {
        mpscq_destroy(&q);
        CASE_FAIL("first dequeued element mismatch");
    }

    if (memcmp(out1, src[1], sizeof(out1)) != 0)
        reproduced = 1;

    mpscq_destroy(&q);
    return reproduced;
}

static int case_capacity_pow2_overflow(void)
{
    mpscq_t q;
    const size_t bad_capacity = 0x80000001ULL;
    int reproduced = 0;

    memset(&q, 0, sizeof(q));
    mpscq_case_error_reset();

    if (mpscq_init(&q, bad_capacity, 8) == 0) {
        mpscq_token_t tok;
        void *p;

        p = mpscq_alloc(&q, 1, &tok);
        if (q.size == 0 && p == NULL)
            reproduced = 1;

        mpscq_destroy(&q);
    }

    return reproduced;
}

static int case_alloc_bytes_push_units_mismatch(void)
{
    mpscq_t q;
    mpscq_token_t tok;
    void *slot;
    uint32_t count;
    int reproduced = 0;

    memset(&q, 0, sizeof(q));
    mpscq_case_error_reset();

    if (mpscq_init(&q, 16, 32) != 0)
        CASE_FAIL("mpscq_init failed");

    slot = mpscq_alloc_bytes(&q, 50, &tok);
    if (!slot) {
        mpscq_destroy(&q);
        CASE_FAIL("mpscq_alloc_bytes(50) failed");
    }

    memset(slot, 0xa5, 50);

    /* 故意误用：push 期望单位是 slot 数，不是 bytes。 */
    mpscq_push(&q, tok, 50);

    count = mpscq_count(&q);
    if (count > q.size)
        reproduced = 1;

    mpscq_destroy(&q);
    return reproduced;
}

static const struct mpscq_issue_case g_cases[] = {
    {
        .name = "cross_boundary_stall",
        .desc = "empty queue can still fail alloc(n>tail_remain) forever",
        .fn = case_cross_boundary_stall,
    },
    {
        .name = "stride_mismatch_batch_copy",
        .desc = "enqueue/dequeue(n>1) data layout mismatches elem_stride",
        .fn = case_stride_mismatch_batch_copy,
    },
    {
        .name = "capacity_pow2_overflow",
        .desc = "next_pow2_u32 overflow can create size=0 queue",
        .fn = case_capacity_pow2_overflow,
    },
    {
        .name = "alloc_bytes_push_units_mismatch",
        .desc = "alloc_bytes + push(bytes) breaks queue accounting",
        .fn = case_alloc_bytes_push_units_mismatch,
    },
};

int mpscq_test_all(void)
{
    int i;
    int reproduced = 0;
    int not_reproduced = 0;
    int errors = 0;

    TEST_LOG("start: %d cases", (int)(sizeof(g_cases) / sizeof(g_cases[0])));

    for (i = 0; i < (int)(sizeof(g_cases) / sizeof(g_cases[0])); i++) {
        int rc = g_cases[i].fn();
        if (rc > 0) {
            reproduced++;
            TEST_LOG("[REPRODUCED] %s: %s", g_cases[i].name, g_cases[i].desc);
        } else if (rc == 0) {
            not_reproduced++;
            TEST_LOG("[NOT-REPRODUCED] %s: %s", g_cases[i].name, g_cases[i].desc);
        } else {
            errors++;
            if (g_last_case_error.func && g_last_case_error.reason) {
                TEST_LOG("[ERROR] %s: %s at %s:%d", g_cases[i].name, g_last_case_error.reason,
                         g_last_case_error.func, g_last_case_error.line);
            } else {
                TEST_LOG("[ERROR] %s: setup/runtime failed (location unknown)", g_cases[i].name);
            }
        }
    }

    TEST_LOG("summary: reproduced=%d not_reproduced=%d errors=%d", reproduced, not_reproduced, errors);
    return errors ? -1 : 0;
}

#define MPSCQ_NET_MAX_PRODUCERS 8
#define MPSCQ_NET_MAX_MSGS_PER_PRODUCER 12000
#define MPSCQ_NET_MAX_PAYLOAD 1536
#define MPSCQ_NET_QUEUE_DEPTH 1024
#define MPSCQ_NET_TASK_STACK 8192
#define MPSCQ_NET_TASK_PRIO 23

enum mpscq_net_core_mode {
    MPSCQ_NET_SINGLE_CORE = 0,
    MPSCQ_NET_MULTI_CORE = 1,
};

struct mpscq_net_profile {
    const char *name;
    uint32_t payload_len;
    uint32_t msg_per_producer;
    uint32_t interval_ticks;
};

struct mpscq_net_msg {
    uint16_t payload_len;
    uint16_t producer_id;
    uint32_t seq;
    uint32_t checksum;
    uint8_t payload[MPSCQ_NET_MAX_PAYLOAD];
};

struct mpscq_net_ctx {
    mpscq_t queue;
    SEMA_ID done_sem;
    TASK_ID consumer_task;
    TASK_ID producer_tasks[MPSCQ_NET_MAX_PRODUCERS];
    uint8_t seen[MPSCQ_NET_MAX_PRODUCERS][MPSCQ_NET_MAX_MSGS_PER_PRODUCER / 8];

    uint32_t producer_count;
    uint32_t payload_len;
    uint32_t msg_per_producer;
    uint32_t no_progress_timeout_ticks;

    atomic_t producer_done;
    atomic_t producer_sent;
    atomic_t enqueue_retry;
    atomic_t consumer_recv;
    atomic_t checksum_err;
    atomic_t format_err;
    atomic_t dup_err;
    atomic_t miss_err;
    atomic_t timeout_err;
};

struct mpscq_net_producer_arg {
    struct mpscq_net_ctx *ctx;
    uint32_t producer_id;
    uint32_t interval_ticks;
};

struct mpscq_net_consumer_arg {
    struct mpscq_net_ctx *ctx;
};

static inline uint32_t mpscq_u32_min(uint32_t a, uint32_t b)
{
    return (a < b) ? a : b;
}

static inline T_UDWORD mpscq_tick_diff(T_UDWORD now, T_UDWORD old)
{
    return now - old;
}

static inline int mpscq_seen_test(const uint8_t *bitmap, uint32_t idx)
{
    return (bitmap[idx >> 3] & (uint8_t)(1u << (idx & 7))) != 0;
}

static inline void mpscq_seen_set(uint8_t *bitmap, uint32_t idx)
{
    bitmap[idx >> 3] |= (uint8_t)(1u << (idx & 7));
}

static uint32_t mpscq_net_checksum(const struct mpscq_net_msg *msg)
{
    uint32_t i;
    uint32_t sum = 0x9e3779b9u ^ (uint32_t)msg->payload_len ^
                   ((uint32_t)msg->producer_id << 16) ^ msg->seq;

    for (i = 0; i < (uint32_t)msg->payload_len; i++) {
        sum = (sum << 5) | (sum >> 27);
        sum ^= msg->payload[i];
    }
    return sum;
}

static void mpscq_net_fill_payload(struct mpscq_net_msg *msg)
{
    uint32_t i;
    uint32_t seed = ((uint32_t)msg->producer_id * 131u) ^ (msg->seq * 17u);

    for (i = 0; i < (uint32_t)msg->payload_len; i++) {
        msg->payload[i] = (uint8_t)((seed + i) & 0xffu);
    }
    msg->checksum = mpscq_net_checksum(msg);
}

static void mpscq_net_producer_task(void *arg)
{
    struct mpscq_net_producer_arg *pa = (struct mpscq_net_producer_arg *)arg;
    struct mpscq_net_ctx *ctx = pa->ctx;
    struct mpscq_net_msg msg;
    uint32_t seq;

    memset(&msg, 0, sizeof(msg));
    msg.payload_len = (uint16_t)ctx->payload_len;
    msg.producer_id = (uint16_t)pa->producer_id;

    for (seq = 0; seq < ctx->msg_per_producer; seq++) {
        msg.seq = seq;
        mpscq_net_fill_payload(&msg);

        while (mpscq_enqueue(&ctx->queue, &msg, 1) != 0) {
            atomic_inc(&ctx->enqueue_retry);
            TTOS_SleepTask(1);
        }

        atomic_inc(&ctx->producer_sent);

        if (pa->interval_ticks > 0) {
            TTOS_SleepTask(pa->interval_ticks);
        }
    }

    atomic_inc(&ctx->producer_done);
    TTOS_ReleaseSema(ctx->done_sem);
    TTOS_SuspendTask(TTOS_SELF_OBJECT_ID);
}

static void mpscq_net_consumer_task(void *arg)
{
    struct mpscq_net_consumer_arg *ca = (struct mpscq_net_consumer_arg *)arg;
    struct mpscq_net_ctx *ctx = ca->ctx;
    struct mpscq_net_msg msg;
    T_UDWORD now_ticks = 0;
    T_UDWORD last_progress_ticks = 0;
    uint32_t expected_total = ctx->producer_count * ctx->msg_per_producer;
    uint32_t i;

    TTOS_GetSystemTicks(&last_progress_ticks);

    while (1) {
        if (mpscq_dequeue(&ctx->queue, &msg, 1) == 0) {
            int format_err = 0;
            uint32_t sum;

            TTOS_GetSystemTicks(&last_progress_ticks);

            if ((uint32_t)msg.producer_id >= ctx->producer_count ||
                msg.seq >= ctx->msg_per_producer ||
                msg.payload_len != ctx->payload_len) {
                format_err = 1;
            }

            if (format_err) {
                atomic_inc(&ctx->format_err);
            } else {
                sum = mpscq_net_checksum(&msg);
                if (sum != msg.checksum) {
                    atomic_inc(&ctx->checksum_err);
                }

                if (mpscq_seen_test(ctx->seen[msg.producer_id], msg.seq)) {
                    atomic_inc(&ctx->dup_err);
                } else {
                    mpscq_seen_set(ctx->seen[msg.producer_id], msg.seq);
                }
            }

            atomic_inc(&ctx->consumer_recv);
            if ((uint32_t)atomic_read(&ctx->consumer_recv) >= expected_total) {
                break;
            }
            continue;
        }

        if ((uint32_t)atomic_read(&ctx->producer_done) >= ctx->producer_count &&
            mpscq_count(&ctx->queue) == 0) {
            break;
        }

        TTOS_SleepTask(1);
        TTOS_GetSystemTicks(&now_ticks);
        if (mpscq_tick_diff(now_ticks, last_progress_ticks) > ctx->no_progress_timeout_ticks) {
            atomic_inc(&ctx->timeout_err);
            break;
        }
    }

    for (i = 0; i < ctx->producer_count; i++) {
        uint32_t seq;
        uint32_t miss = 0;
        for (seq = 0; seq < ctx->msg_per_producer; seq++) {
            if (!mpscq_seen_test(ctx->seen[i], seq)) {
                miss++;
            }
        }
        if (miss) {
            atomic_add(&ctx->miss_err, (long)miss);
        }
    }

    TTOS_ReleaseSema(ctx->done_sem);
    TTOS_SuspendTask(TTOS_SELF_OBJECT_ID);
}

static int mpscq_get_enabled_cpu_list(uint32_t *cpus, uint32_t max_cpus)
{
    uint32_t count = 0;
#ifdef CONFIG_SMP
    uint32_t i;
    for (i = 0; i < CONFIG_MAX_CPUS && count < max_cpus; i++) {
        if (CPU_ISSET(i, TTOS_CPUSET_ENABLED())) {
            cpus[count++] = i;
        }
    }
#else
    (void)max_cpus;
#endif
    if (count == 0) {
        cpus[0] = (uint32_t)cpuid_get();
        count = 1;
    }
    return (int)count;
}

static void mpscq_bind_task_cpu(TASK_ID task, uint32_t cpu_id)
{
#ifdef CONFIG_SMP
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset);
    (void)TTOS_SetTaskAffinity(task, &cpuset);
#else
    (void)task;
    (void)cpu_id;
#endif
}

static int mpscq_create_task(const char *name, T_TTOS_TaskRoutine entry, void *arg, TASK_ID *out)
{
    T_TTOS_ReturnCode ret;
    ret = TTOS_CreateTaskEx((T_UBYTE *)name, MPSCQ_NET_TASK_PRIO, FALSE, TRUE, entry, arg,
                            MPSCQ_NET_TASK_STACK, out);
    if (ret != TTOS_OK) {
        TEST_LOG("[ERROR] create task %s failed ret=%d", name, ret);
        return -1;
    }
    return 0;
}

static int mpscq_run_lwip_net_profile(const struct mpscq_net_profile *profile,
                                      enum mpscq_net_core_mode core_mode)
{
    struct mpscq_net_ctx ctx;
    struct mpscq_net_consumer_arg consumer_arg;
    struct mpscq_net_producer_arg producer_args[MPSCQ_NET_MAX_PRODUCERS];
    uint32_t cpu_list[MPSCQ_NET_MAX_PRODUCERS + 1];
    uint32_t cpu_count;
    uint32_t producer_count;
    uint32_t i;
    uint32_t waiters;
    T_UDWORD start_ticks = 0;
    T_UDWORD end_ticks = 0;
    uint32_t ticks_per_sec = TTOS_GetSysClkRate();
    int rc = 0;

    memset(&ctx, 0, sizeof(ctx));
    memset(&consumer_arg, 0, sizeof(consumer_arg));
    memset(producer_args, 0, sizeof(producer_args));

    if (profile->payload_len == 0 || profile->payload_len > MPSCQ_NET_MAX_PAYLOAD) {
        TEST_LOG("[ERROR] invalid payload_len=%u for %s", profile->payload_len, profile->name);
        return -1;
    }
    if (profile->msg_per_producer == 0 || profile->msg_per_producer > MPSCQ_NET_MAX_MSGS_PER_PRODUCER) {
        TEST_LOG("[ERROR] invalid msg_per_producer=%u for %s",
                 profile->msg_per_producer, profile->name);
        return -1;
    }

    cpu_count = (uint32_t)mpscq_get_enabled_cpu_list(cpu_list, MPSCQ_NET_MAX_PRODUCERS + 1);
    if (core_mode == MPSCQ_NET_MULTI_CORE && cpu_count < 2) {
        TEST_LOG("[SKIP] %s multi-core: enabled cpu count=%u", profile->name, cpu_count);
        return 0;
    }

    producer_count = (core_mode == MPSCQ_NET_MULTI_CORE) ? mpscq_u32_min(cpu_count - 1, 4u) : 3u;
    if (producer_count == 0) {
        producer_count = 1;
    }
    if (producer_count > MPSCQ_NET_MAX_PRODUCERS) {
        producer_count = MPSCQ_NET_MAX_PRODUCERS;
    }

    ctx.producer_count = producer_count;
    ctx.payload_len = profile->payload_len;
    ctx.msg_per_producer = profile->msg_per_producer;
    ctx.no_progress_timeout_ticks = ticks_per_sec * 30u;
    atomic_write(&ctx.producer_done, 0);
    atomic_write(&ctx.producer_sent, 0);
    atomic_write(&ctx.enqueue_retry, 0);
    atomic_write(&ctx.consumer_recv, 0);
    atomic_write(&ctx.checksum_err, 0);
    atomic_write(&ctx.format_err, 0);
    atomic_write(&ctx.dup_err, 0);
    atomic_write(&ctx.miss_err, 0);
    atomic_write(&ctx.timeout_err, 0);

    if (mpscq_init(&ctx.queue, MPSCQ_NET_QUEUE_DEPTH, sizeof(struct mpscq_net_msg)) != 0) {
        TEST_LOG("[ERROR] %s init queue failed", profile->name);
        return -1;
    }

    if (TTOS_CreateSemaEx(0, &ctx.done_sem) != TTOS_OK) {
        TEST_LOG("[ERROR] %s create done sem failed", profile->name);
        mpscq_destroy(&ctx.queue);
        return -1;
    }

    consumer_arg.ctx = &ctx;
    if (mpscq_create_task("mpscq_net_c", (T_TTOS_TaskRoutine)mpscq_net_consumer_task,
                          &consumer_arg, &ctx.consumer_task) != 0) {
        TTOS_DeleteSema(ctx.done_sem);
        mpscq_destroy(&ctx.queue);
        return -1;
    }

    for (i = 0; i < producer_count; i++) {
        producer_args[i].ctx = &ctx;
        producer_args[i].producer_id = i;
        producer_args[i].interval_ticks = profile->interval_ticks;
        if (mpscq_create_task("mpscq_net_p", (T_TTOS_TaskRoutine)mpscq_net_producer_task,
                              &producer_args[i], &ctx.producer_tasks[i]) != 0) {
            rc = -1;
            goto cleanup;
        }
    }

    if (core_mode == MPSCQ_NET_SINGLE_CORE) {
        uint32_t target = cpu_list[0];
        mpscq_bind_task_cpu(ctx.consumer_task, target);
        for (i = 0; i < producer_count; i++) {
            mpscq_bind_task_cpu(ctx.producer_tasks[i], target);
        }
    } else {
        mpscq_bind_task_cpu(ctx.consumer_task, cpu_list[0]);
        for (i = 0; i < producer_count; i++) {
            mpscq_bind_task_cpu(ctx.producer_tasks[i], cpu_list[(i + 1) % cpu_count]);
        }
    }

    TTOS_GetSystemTicks(&start_ticks);
    TTOS_ActiveTask(ctx.consumer_task);
    for (i = 0; i < producer_count; i++) {
        TTOS_ActiveTask(ctx.producer_tasks[i]);
    }

    waiters = producer_count + 1;
    for (i = 0; i < waiters; i++) {
        T_TTOS_ReturnCode ret = TTOS_ObtainSema(ctx.done_sem, ticks_per_sec * 60u);
        if (ret != TTOS_OK) {
            TEST_LOG("[ERROR] %s wait task done timeout/err=%d", profile->name, ret);
            rc = -1;
            break;
        }
    }
    TTOS_GetSystemTicks(&end_ticks);

    {
        uint32_t expected = producer_count * profile->msg_per_producer;
        uint32_t recv = (uint32_t)atomic_read(&ctx.consumer_recv);
        uint32_t sent = (uint32_t)atomic_read(&ctx.producer_sent);
        uint32_t csum_err = (uint32_t)atomic_read(&ctx.checksum_err);
        uint32_t fmt_err = (uint32_t)atomic_read(&ctx.format_err);
        uint32_t dup_err = (uint32_t)atomic_read(&ctx.dup_err);
        uint32_t miss_err = (uint32_t)atomic_read(&ctx.miss_err);
        uint32_t tout_err = (uint32_t)atomic_read(&ctx.timeout_err);
        uint32_t retry = (uint32_t)atomic_read(&ctx.enqueue_retry);
        T_UDWORD elapsed_ticks = mpscq_tick_diff(end_ticks, start_ticks);
        T_UDWORD elapsed_ms = (ticks_per_sec != 0u) ? (elapsed_ticks * 1000u / ticks_per_sec) : 0u;

        TEST_LOG("[NET] %-24s core=%s producers=%u payload=%u msg/prod=%u elapsed=%llums sent=%u recv=%u retry=%u err(csum=%u fmt=%u dup=%u miss=%u timeout=%u)",
                 profile->name,
                 (core_mode == MPSCQ_NET_SINGLE_CORE) ? "single" : "multi",
                 producer_count,
                 profile->payload_len,
                 profile->msg_per_producer,
                 (unsigned long long)elapsed_ms,
                 sent,
                 recv,
                 retry,
                 csum_err,
                 fmt_err,
                 dup_err,
                 miss_err,
                 tout_err);

        if (sent != expected || recv != expected || csum_err != 0 || fmt_err != 0 ||
            dup_err != 0 || miss_err != 0 || tout_err != 0) {
            rc = -1;
        }
    }

cleanup:
    if (ctx.consumer_task) {
        (void)TTOS_DeleteTask(ctx.consumer_task);
    }
    for (i = 0; i < producer_count; i++) {
        if (ctx.producer_tasks[i]) {
            (void)TTOS_DeleteTask(ctx.producer_tasks[i]);
        }
    }
    if (ctx.done_sem) {
        TTOS_DeleteSema(ctx.done_sem);
    }
    mpscq_destroy(&ctx.queue);
    return rc;
}

int mpscq_test_lwip_net_all(void)
{
    static const struct mpscq_net_profile profiles[] = {
        { "lowfreq_lowdata",  64,   600, 2 },
        { "lowfreq_highdata", 1400, 900, 2 },
        { "highfreq_lowdata", 64,   6000, 0 },
        { "highfreq_highdata",1400, 3500, 0 },
    };
    int i;
    int fails = 0;

    TEST_LOG("[NET] start: %d traffic profiles x single/multi core",
             (int)(sizeof(profiles) / sizeof(profiles[0])));

    for (i = 0; i < (int)(sizeof(profiles) / sizeof(profiles[0])); i++) {
        if (mpscq_run_lwip_net_profile(&profiles[i], MPSCQ_NET_SINGLE_CORE) != 0) {
            fails++;
        }
        if (mpscq_run_lwip_net_profile(&profiles[i], MPSCQ_NET_MULTI_CORE) != 0) {
            fails++;
        }
    }

    TEST_LOG("[NET] done: fail_cases=%d", fails);
    return fails ? -1 : 0;
}

#ifdef CONFIG_SHELL
#include <shell.h>

static void mpscq_test_cmd(int argc, const char *argv[])
{
    if (argc >= 2) {
        if (strcmp(argv[1], "repro") == 0) {
            (void)mpscq_test_all();
            return;
        }
        if (strcmp(argv[1], "net") == 0) {
            (void)mpscq_test_lwip_net_all();
            return;
        }
        if (strcmp(argv[1], "all") == 0) {
            (void)mpscq_test_all();
            (void)mpscq_test_lwip_net_all();
            return;
        }
    }

    (void)mpscq_test_lwip_net_all();
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) |
                     SHELL_CMD_DISABLE_RETURN,
                 mpscq_test, mpscq_test_cmd, mpscq repro and net stress test);
#endif
