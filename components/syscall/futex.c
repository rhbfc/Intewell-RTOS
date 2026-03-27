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

#include "syscall_internal.h"
#include <assert.h>
#include <atomic.h>
#include <errno.h>
#include <futex.h>
#include <hash.h>
#include <kmalloc.h>
#include <stddef.h>
#include <string.h>
#include <tid.h>
#include <time.h>
#include <time/ktime.h>
#include <ttosBase.h>
#include <ttosProcess.h>
#include <ttos_init.h>
#include <uaccess.h>

#define FUTEX_FLAGS (FUTEX_PRIVATE_FLAG | FUTEX_CLOCK_REALTIME)

#define FUTEX_HASH_BUCKET 2048
#define FUTEX_PI_POOL_SIZE 64
#define FUTEX_ROBUST_LIST_LIMIT 1024
#define FUTEX_Q_FLAG_WAIT_REQUEUE_PI (1U << 0)

struct futex_bucket
{
    ttos_spinlock_t lock;
    struct list_head chain;
    struct list_head pi_chain;
};

struct futex_bucket futex_table[FUTEX_HASH_BUCKET];
static ttos_spinlock_t futex_pi_pool_lock;
static struct list_head futex_pi_free_list;
static struct list_head futex_pi_lru_list;
static struct futex_pi_state futex_pi_pool[FUTEX_PI_POOL_SIZE];

/*
 * Defensively validate list links before list_add/list_del on task-owned rt_mutex
 * chains. This prevents a corrupted list pointer from turning into a kernel DABT.
 */
static bool futex_list_head_sane(struct list_head *head)
{
    if (!head || !kernel_access_check(head, sizeof(*head), UACCESS_RW))
    {
        return false;
    }

    if (!head->next || !head->prev)
    {
        return false;
    }

    if (!kernel_access_check(head->next, sizeof(*head), UACCESS_RW) ||
        !kernel_access_check(head->prev, sizeof(*head), UACCESS_RW))
    {
        return false;
    }

    return true;
}

static bool futex_list_node_sane(struct list_head *node)
{
    if (!node || !kernel_access_check(node, sizeof(*node), UACCESS_RW))
    {
        return false;
    }

    if (node->next == node && node->prev == node)
    {
        return true;
    }

    if (!node->next || !node->prev)
    {
        return false;
    }

    if (!kernel_access_check(node->next, sizeof(*node), UACCESS_RW) ||
        !kernel_access_check(node->prev, sizeof(*node), UACCESS_RW))
    {
        return false;
    }

    return true;
}

static bool futex_owner_task_sane(TASK_ID task, int tid_hint)
{
    if (!task || !kernel_access_check(task, sizeof(*task), UACCESS_RW))
    {
        return false;
    }

    /*
     * taskControlId points to itself for a live task; recycled/cleared TCBs
     * usually break this invariant first.
     */
    if (task->taskControlId != task)
    {
        return false;
    }

    if (tid_hint > 0 && task->tid != tid_hint)
    {
        return false;
    }

    return futex_list_head_sane(&task->held_rt_mutexs);
}

static int futex_owner_node_detach_safe(struct futex_pi_state *s)
{
    struct list_head *node = &s->pi->owner_node;

    if (!futex_list_node_sane(node))
    {
        return -EFAULT;
    }

    if (!list_empty(node))
    {
        list_del_init(node);
    }

    return 0;
}

static int futex_owner_node_attach_safe(struct futex_pi_state *s, TASK_ID owner_task)
{
    if (!s || !s->pi || !owner_task)
    {
        return -EINVAL;
    }

    struct list_head *head = &owner_task->held_rt_mutexs;
    struct list_head *node = &s->pi->owner_node;

    if (!kernel_access_check(owner_task, sizeof(*owner_task), UACCESS_RW))
    {
        return -EFAULT;
    }

    /*
     * If list links are transiently clobbered, reinitialize local list heads
     * and fail closed instead of taking a kernel DABT in list_add().
     */
    if (head->next == NULL || head->prev == NULL)
    {
        INIT_LIST_HEAD(head);
    }
    if (node->next == NULL || node->prev == NULL)
    {
        INIT_LIST_HEAD(node);
    }

    if (!futex_list_head_sane(head) || !futex_list_node_sane(node))
    {
        return -EFAULT;
    }

    if (list_empty(node))
    {
        list_add(node, head);
    }

    return 0;
}

static void futex_bucket_pair_lock(struct futex_bucket *bucket1, struct futex_bucket *bucket2,
                                   unsigned long *flags)
{
    if (bucket1 == bucket2)
    {
        spin_lock_irqsave(&bucket1->lock, *flags);
        return;
    }

    if (bucket1 < bucket2)
    {
        spin_lock_irqsave(&bucket1->lock, *flags);
        spin_lock(&bucket2->lock);
        return;
    }

    spin_lock_irqsave(&bucket2->lock, *flags);
    spin_lock(&bucket1->lock);
}

static void futex_bucket_pair_unlock(struct futex_bucket *bucket1, struct futex_bucket *bucket2,
                                     unsigned long flags)
{
    if (bucket1 == bucket2)
    {
        spin_unlock_irqrestore(&bucket1->lock, flags);
        return;
    }

    if (bucket1 < bucket2)
    {
        spin_unlock(&bucket2->lock);
        spin_unlock_irqrestore(&bucket1->lock, flags);
        return;
    }

    spin_unlock(&bucket1->lock);
    spin_unlock_irqrestore(&bucket2->lock, flags);
}

static int futex_init(void)
{
    int i = 0;
    for (; i < FUTEX_HASH_BUCKET; i++)
    {
        spin_lock_init(&futex_table[i].lock);
        INIT_LIST_HEAD(&futex_table[i].chain);
        INIT_LIST_HEAD(&futex_table[i].pi_chain);
    }

    spin_lock_init(&futex_pi_pool_lock);
    INIT_LIST_HEAD(&futex_pi_free_list);
    INIT_LIST_HEAD(&futex_pi_lru_list);
    for (i = 0; i < FUTEX_PI_POOL_SIZE; i++)
    {
        struct futex_pi_state *s = &futex_pi_pool[i];
        memset(s, 0, sizeof(*s));
        INIT_LIST_HEAD(&s->pi_list);
        list_add_tail(&s->pi_list, &futex_pi_free_list);
    }
}
INIT_EXPORT_PRE(futex_init, "futex_init");

/**
 * @brief 获取64位数据在指定桶中的索引
 *
 * @param key 64位数据
 * @param bucket_count 桶的数量
 * @return u32 索引
 */
static inline int futex_hash_bucket_get(struct futex_key *key)
{
    uint64_t key64 = key->pid ^ key->uaddr;
    u64 h = hash_64(key64, 0); // splitmix64
    return h & (FUTEX_HASH_BUCKET - 1);
}

static inline bool futex_pi_state_is_pool(struct futex_pi_state *s)
{
    return (s >= futex_pi_pool) && (s < (futex_pi_pool + FUTEX_PI_POOL_SIZE));
}

static struct futex_pi_state *futex_pi_state_alloc(void)
{
    struct futex_pi_state *s = NULL;
    unsigned long flags;

    spin_lock_irqsave(&futex_pi_pool_lock, flags);
    if (!list_empty(&futex_pi_free_list))
    {
        s = list_first_entry(&futex_pi_free_list, struct futex_pi_state, pi_list);
        list_del_init(&s->pi_list);
    }
    else if (!list_empty(&futex_pi_lru_list))
    {
        s = list_first_entry(&futex_pi_lru_list, struct futex_pi_state, pi_list);
        list_del_init(&s->pi_list);
    }
    spin_unlock_irqrestore(&futex_pi_pool_lock, flags);

    if (!s)
    {
        s = kmalloc(sizeof(*s), GFP_KERNEL);
        if (!s)
        {
            return NULL;
        }
    }

    memset(s, 0, sizeof(*s));
    INIT_LIST_HEAD(&s->pi_list);
    rt_mutex_init(&s->rt_mutex);
    s->pi = &s->rt_mutex;
    return s;
}

static void futex_pi_state_to_lru(struct futex_pi_state *s)
{
    unsigned long flags;

    if (!s)
    {
        return;
    }

    if (!futex_pi_state_is_pool(s))
    {
        kfree(s);
        return;
    }

    spin_lock_irqsave(&futex_pi_pool_lock, flags);
    list_add_tail(&s->pi_list, &futex_pi_lru_list);
    spin_unlock_irqrestore(&futex_pi_pool_lock, flags);
}

static struct futex_pi_state *futex_pi_state_lookup_locked(struct futex_bucket *bucket,
                                                           struct futex_key *key)
{
    struct futex_pi_state *s;

    list_for_each_entry(s, &bucket->pi_chain, pi_list)
    {
        if (s->key.pid == key->pid && s->key.uaddr == key->uaddr)
        {
            return s;
        }
    }
    return NULL;
}

static struct futex_pi_state *futex_pi_state_get(struct futex_key *key, bool create)
{
    struct futex_bucket *bucket;
    struct futex_pi_state *s;
    struct futex_pi_state *exist;
    unsigned long flags;

    bucket = &futex_table[futex_hash_bucket_get(key)];

    spin_lock_irqsave(&bucket->lock, flags);
    s = futex_pi_state_lookup_locked(bucket, key);
    if (s)
    {
        s->refcount++;
        spin_unlock_irqrestore(&bucket->lock, flags);
        return s;
    }
    spin_unlock_irqrestore(&bucket->lock, flags);

    if (!create)
    {
        return NULL;
    }

    s = futex_pi_state_alloc();
    if (!s)
    {
        return NULL;
    }

    s->key = *key;
    s->refcount = 1;

    spin_lock_irqsave(&bucket->lock, flags);
    /* 可能在分配期间被别人创建 */
    exist = futex_pi_state_lookup_locked(bucket, key);
    if (exist)
    {
        exist->refcount++;
        spin_unlock_irqrestore(&bucket->lock, flags);
        futex_pi_state_to_lru(s);
        return exist;
    }
    list_add_tail(&s->pi_list, &bucket->pi_chain);
    spin_unlock_irqrestore(&bucket->lock, flags);

    return s;
}

static void futex_pi_state_put(struct futex_pi_state *s)
{
    struct futex_bucket *bucket;
    unsigned long flags;

    if (!s)
    {
        return;
    }

    bucket = &futex_table[futex_hash_bucket_get(&s->key)];

    spin_lock_irqsave(&bucket->lock, flags);
    if (s->refcount > 0)
    {
        s->refcount--;
    }
    if (s->refcount == 0 && s->owner == NULL && s->waiters == 0)
    {
        list_del_init(&s->pi_list);
        spin_unlock_irqrestore(&bucket->lock, flags);
        futex_pi_state_to_lru(s);
        return;
    }
    spin_unlock_irqrestore(&bucket->lock, flags);
}

static bool futex_timeout_expired(const struct timespec *timeout, bool is_absolute_timeout,
                                  bool use_realtime_clock)
{
    struct timespec64 now_ts;
    clockid_t clockid;

    if (!timeout)
    {
        return false;
    }

    if (!is_absolute_timeout)
    {
        return timeout->tv_sec == 0 && timeout->tv_nsec == 0;
    }

    clockid = use_realtime_clock ? CLOCK_REALTIME : CLOCK_MONOTONIC;
    kernel_clock_gettime(clockid, &now_ts);

    return (now_ts.tv_sec > timeout->tv_sec) ||
           ((now_ts.tv_sec == timeout->tv_sec) && (now_ts.tv_nsec > timeout->tv_nsec));
}

static int futex_timeout_to_tick(const struct timespec *timeout, bool is_absolute_timeout,
                                 bool use_realtime_clock)
{
    struct timespec64 now64;
    struct timespec now;
    struct timespec rel;

    if (!timeout)
    {
        return TTOS_WAIT_FOREVER;
    }

    if (!is_absolute_timeout)
    {
        return clock_time_to_tick(timeout, false);
    }

    if (use_realtime_clock)
    {
        return clock_time_to_tick(timeout, true);
    }

    kernel_clock_gettime(CLOCK_MONOTONIC, &now64);
    now.tv_sec = now64.tv_sec;
    now.tv_nsec = now64.tv_nsec;
    rel = clock_timespec_subtract(timeout, &now);

    return clock_time_to_tick(&rel, false);
}

static int futex_wait(struct futex_key *key, u32 value, const struct timespec *timeout, u32 bitset,
                      int op_flags, bool is_absolute_timeout)
{
    struct futex_bucket *bucket;
    struct futex_q *q;
    unsigned long flags;
    u32 uval;
    bool has_timeout = false;
    bool timeout_expired;
    bool use_realtime_clock = (op_flags & FUTEX_CLOCK_REALTIME) != 0;

    /* bitset 必须非零 */
    if (bitset == 0)
    {
        return -EINVAL;
    }

    /* 验证用户空间地址 */
    if (!user_access_check((void *)(uintptr_t)key->uaddr, sizeof(int), UACCESS_R))
    {
        return -EFAULT;
    }

    /* 处理超时参数 */
    if (timeout != NULL)
    {
        /* 验证 timespec 合法性 */
        if (timeout->tv_sec < 0 || timeout->tv_nsec < 0 || timeout->tv_nsec >= 1000000000)
        {
            return -EINVAL;
        }
        has_timeout = true;
    }
    timeout_expired = futex_timeout_expired(timeout, is_absolute_timeout, use_realtime_clock);

    q = &current->futex;
    bucket = &futex_table[futex_hash_bucket_get(key)];

    /* @KEEP_COMMENT: 禁止调度器 */
    ttosDisableTaskDispatchWithLock();

    /*
     * 关键修复：先拿锁，再比较，再入队
     * 保证 compare-and-block 的原子性，防止 lost wake-up
     */
    spin_lock_irqsave(&bucket->lock, flags);

    /* 在锁保护下读取并比较 futex word，如果不匹配，返回 EAGAIN */
    uval = atomic32_read((atomic32_t *)(uintptr_t)key->uaddr);
    if (uval != value)
    {
        spin_unlock_irqrestore(&bucket->lock, flags);

        /* @KEEP_COMMENT: 重新使能调度 */
        ttosEnableTaskDispatchWithLock();

        /*
         * 根据 man page: EAGAIN 优先于 ETIMEDOUT
         * "The value pointed to by uaddr was not equal to the expected
         *  value val at the time of the call."
         */
        return -EAGAIN;
    }

    /*
     * value 匹配，但如果超时已经过期，返回 ETIMEDOUT
     * 这个检查必须在 value 检查之后
     */
    if (timeout_expired)
    {
        spin_unlock_irqrestore(&bucket->lock, flags);
        /* @KEEP_COMMENT: 重新使能调度 */
        ttosEnableTaskDispatchWithLock();

        return -ETIMEDOUT;
    }

    /* 入队 */
    q->bitset = bitset;
    q->pcb = current;
    q->key = *key;
    q->pi_state = NULL;
    q->flags = 0;
    q->pcb->taskControlId->wait.returnCode = 0;

    INIT_LIST_HEAD(&q->list);

    list_add_head(&q->list, &bucket->chain);
    (void)ttosClearTaskReady(q->pcb->taskControlId);
    q->pcb->taskControlId->state = TTOS_TASK_INTERRUPTIBLE_BY_SIGNAL | TTOS_TASK_WAITING_FOR_FUTEX;
    spin_unlock(&bucket->lock);

    /* 设置超时定时器（在入队之后） */
    if (has_timeout)
    {
        /* @KEEP_COMMENT: 设置当前运行任务的等待时间为<ticks> */
        q->pcb->taskControlId->waitedTicks =
            futex_timeout_to_tick(timeout, is_absolute_timeout, use_realtime_clock);
        /*
         * @KEEP_COMMENT: 调用ttosInsertWaitedTask(DT.2.26)将当前运行任务
         * 插入任务tick等待队列
         */
        (void)ttosInsertWaitedTask(q->pcb->taskControlId);
    }

    /* @KEEP_COMMENT: 重新使能调度 */
    ttosEnableTaskDispatchWithLock();

    ttos_int_unlock(flags);

    // if (has_timeout) {
    //     timer_cancel(current->timer);
    // }

    if (q->pcb->taskControlId->wait.returnCode)
    {
        spin_lock_irqsave(&bucket->lock, flags);
        list_del_init(&q->list);
        spin_unlock_irqrestore(&bucket->lock, flags);
    }
    return -q->pcb->taskControlId->wait.returnCode;
}

static int futex_wake(struct futex_key *key, int number, u32 bitset)
{
    struct futex_bucket *bucket;
    struct futex_q *q;
    struct futex_q *tmp;
    unsigned long flags;
    int wake_count = 0;

    /* bitset 必须非零 */
    if (bitset == 0)
    {
        return -EINVAL;
    }

    bucket = &futex_table[futex_hash_bucket_get(key)];

    /* @KEEP_COMMENT: 禁止调度器 */
    ttosDisableTaskDispatchWithLock();

    spin_lock_irqsave(&bucket->lock, flags);

    list_for_each_entry_safe(q, tmp, &bucket->chain, list)
    {
        if (q->key.pid == key->pid && q->key.uaddr == key->uaddr && (q->bitset & bitset))
        {
            list_del_init(&q->list);
            q->pcb->taskControlId->state &=
                ~(TTOS_TASK_INTERRUPTIBLE_BY_SIGNAL | TTOS_TASK_WAITING_FOR_FUTEX);
            if (q->pcb->taskControlId->objCore.objectNode.next != NULL)
            {
                ttosExactWaitedTask(q->pcb->taskControlId);
            }
            ttosClearTaskWaiting(q->pcb->taskControlId);
            wake_count++;
            if (number > 0 && wake_count >= number)
            {
                break;
            }
        }
    }
    spin_unlock_irqrestore(&bucket->lock, flags);

    /* @KEEP_COMMENT: 重新使能调度 */
    ttosEnableTaskDispatchWithLock();

    return wake_count;
}

/**
 * @brief 比较和交换kfutex结构体的值。
 *
 * @param curval 当前值。
 * @param uaddr 用户空间地址。
 * @param uval 用户值。
 * @param newval 新值。
 * @return 比较和交换结果。
 */

static inline int _futex_cmpxchg_value(int *curval, uint32_t *uaddr, int uval, int newval)
{
    int ret = 0;

    if (!user_access_check((void *)uaddr, sizeof(*uaddr), UACCESS_RW))
    {
        ret = -EFAULT;
        goto exit;
    }

    if (!atomic32_cas((atomic32_t *)uaddr, uval, newval))
    {
        *curval = uval;
        ret = -EAGAIN;
    }

exit:
    return ret;
}

static int futex_requeue(struct futex_key *key1, struct futex_key *key2, int num_wake,
                         int num_requeue)
{
    int woken_cnt = 0;
    int req_cnt = 0;
    struct futex_q *q;
    struct futex_q *tmp;
    unsigned long flags;
    struct futex_bucket *bucket1, *bucket2;

    if (key1->pid == key2->pid && key1->uaddr == key2->uaddr)
    {
        return -EINVAL;
    }

    bucket1 = &futex_table[futex_hash_bucket_get(key1)];
    bucket2 = &futex_table[futex_hash_bucket_get(key2)];

    /* @KEEP_COMMENT: 禁止调度器 */
    ttosDisableTaskDispatchWithLock();

    futex_bucket_pair_lock(bucket1, bucket2, &flags);

    list_for_each_entry_safe(q, tmp, &bucket1->chain, list)
    {
        if (q->key.pid == key1->pid && q->key.uaddr == key1->uaddr)
        {
            list_del_init(&q->list);
            q->pcb->taskControlId->state &=
                ~(TTOS_TASK_INTERRUPTIBLE_BY_SIGNAL | TTOS_TASK_WAITING_FOR_FUTEX);
            if (q->pcb->taskControlId->objCore.objectNode.next != NULL)
            {
                ttosExactWaitedTask(q->pcb->taskControlId);
            }
            ttosClearTaskWaiting(q->pcb->taskControlId);
            woken_cnt++;
            if (num_wake > 0 && woken_cnt >= num_wake)
            {
                break;
            }
        }
    }
    list_for_each_entry_safe(q, tmp, &bucket1->chain, list)
    {
        if (q->key.pid == key1->pid && q->key.uaddr == key1->uaddr)
        {
            list_del_init(&q->list);
            q->key = *key2;
            list_add_tail(&q->list, &bucket2->chain);

            req_cnt++;
            if (num_requeue > 0 && req_cnt >= num_requeue)
            {
                break;
            }
        }
    }
    futex_bucket_pair_unlock(bucket1, bucket2, flags);

    /* @KEEP_COMMENT: 重新使能调度 */
    ttosEnableTaskDispatchWithLock();

    return woken_cnt + req_cnt;
}

static void futex_pi_state_set_owner(struct futex_pi_state *s, TASK_ID owner)
{
    struct futex_bucket *bucket;
    unsigned long flags;

    if (!s)
    {
        return;
    }

    bucket = &futex_table[futex_hash_bucket_get(&s->key)];
    spin_lock_irqsave(&bucket->lock, flags);
    s->owner = owner;
    s->owner_tid = owner ? owner->tid : 0;
    spin_unlock_irqrestore(&bucket->lock, flags);
}

static void futex_pi_state_inc_waiters(struct futex_pi_state *s)
{
    struct futex_bucket *bucket;
    unsigned long flags;

    if (!s)
    {
        return;
    }

    bucket = &futex_table[futex_hash_bucket_get(&s->key)];
    spin_lock_irqsave(&bucket->lock, flags);
    s->waiters++;
    spin_unlock_irqrestore(&bucket->lock, flags);
}

static void futex_pi_state_dec_waiters(struct futex_pi_state *s)
{
    struct futex_bucket *bucket;
    unsigned long flags;

    if (!s)
    {
        return;
    }

    bucket = &futex_table[futex_hash_bucket_get(&s->key)];
    spin_lock_irqsave(&bucket->lock, flags);
    if (s->waiters > 0)
    {
        s->waiters--;
    }
    spin_unlock_irqrestore(&bucket->lock, flags);
}

static int futex_lock_pi(struct futex_key *key, uint32_t *uaddr, const struct timespec *timeout,
                         int op_flags, bool trylock)
{
    int word;
    int cword;
    int tid;
    int ret;
    unsigned int ticks = TTOS_WAIT_FOREVER;
    bool timeout_expired = false;
    bool is_absolute = true;
    bool sched_lock_held = false;
    struct futex_pi_state *pi_state = NULL;
    TASK_ID owner_task = NULL;
    T_TTOS_TaskControlBlock *cur_tcb = ttosGetRunningTask();

    (void)op_flags;

    if (!user_access_check(uaddr, sizeof(*uaddr), UACCESS_RW))
    {
        return -EFAULT;
    }

    if (timeout != NULL)
    {
        if (timeout->tv_sec < 0 || timeout->tv_nsec < 0 || timeout->tv_nsec >= 1000000000)
        {
            return -EINVAL;
        }
        if (is_absolute)
        {
            struct timespec64 now_ts;
            kernel_clock_gettime(CLOCK_REALTIME, &now_ts);

            if ((now_ts.tv_sec > timeout->tv_sec) ||
                ((now_ts.tv_sec == timeout->tv_sec) && (now_ts.tv_nsec > timeout->tv_nsec)))
            {
                timeout_expired = true;
            }
        }
        ticks = clock_time_to_tick(timeout, is_absolute);
    }

retry:
    word = atomic32_read((atomic32_t *)uaddr);
    tid = word & FUTEX_TID_MASK;

    if (tid == 0)
    {
        int new_word = (word & ~FUTEX_TID_MASK) | cur_tcb->tid;
        if (_futex_cmpxchg_value(&cword, uaddr, word, new_word))
        {
            goto retry;
        }
        return 0;
    }

    if (tid == cur_tcb->tid)
    {
        return -EDEADLK;
    }

    if (trylock)
    {
        return -EBUSY;
    }

    if (timeout_expired)
    {
        return -ETIMEDOUT;
    }

    if (!(word & FUTEX_WAITERS))
    {
        int nword = word | FUTEX_WAITERS;
        if (_futex_cmpxchg_value(&cword, uaddr, word, nword))
        {
            goto retry;
        }
        word = nword;
        tid = word & FUTEX_TID_MASK;
    }

    owner_task = task_get_by_tid(tid);
    if (!owner_task || !futex_owner_task_sane(owner_task, tid))
    {
        return -ESRCH;
    }

    pi_state = futex_pi_state_get(key, true);
    if (!pi_state)
    {
        return -ENOMEM;
    }

    ttosDisableTaskDispatchWithLock();
    sched_lock_held = true;

    if (!futex_owner_task_sane(owner_task, tid))
    {
        ret = -ESRCH;
        goto out_unlock_put;
    }

    if (pi_state->owner != owner_task)
    {
        if (pi_state->owner && !futex_owner_task_sane(pi_state->owner, pi_state->owner_tid))
        {
            INIT_LIST_HEAD(&pi_state->pi->owner_node);
            pi_state->pi->owner = NULL;
            futex_pi_state_set_owner(pi_state, NULL);
        }

        if (pi_state->owner)
        {
            unsigned long flags1;
            int nret;
            spin_lock_irqsave(&pi_state->owner->held_rt_mutexs_lock, flags1);
            nret = futex_owner_node_detach_safe(pi_state);
            spin_unlock_irqrestore(&pi_state->owner->held_rt_mutexs_lock, flags1);
            if (nret)
            {
                ret = nret;
                goto out_unlock_put;
            }
        }

        if (!futex_owner_task_sane(owner_task, tid))
        {
            ret = -ESRCH;
            goto out_unlock_put;
        }

        unsigned long flags2;
        int nret;
        spin_lock_irqsave(&owner_task->held_rt_mutexs_lock, flags2);
        nret = futex_owner_node_attach_safe(pi_state, owner_task);
        spin_unlock_irqrestore(&owner_task->held_rt_mutexs_lock, flags2);
        if (nret)
        {
            ret = nret;
            goto out_unlock_put;
        }

        pi_state->pi->owner = owner_task;
        futex_pi_state_set_owner(pi_state, owner_task);
    }

    /*
     * owner 可能在我们建立 PI 关联期间已解锁/转移，避免在陈旧 owner 上永远阻塞。
     */
    word = atomic32_read((atomic32_t *)uaddr);
    if ((word & FUTEX_TID_MASK) != tid)
    {
        if (pi_state->waiters == 0 && pi_state->owner == owner_task &&
            pi_state->pi->owner == owner_task)
        {
            unsigned long flags3;
            int nret;

            spin_lock_irqsave(&owner_task->held_rt_mutexs_lock, flags3);
            nret = futex_owner_node_detach_safe(pi_state);
            spin_unlock_irqrestore(&owner_task->held_rt_mutexs_lock, flags3);
            if (!nret)
            {
                pi_state->pi->owner = NULL;
                futex_pi_state_set_owner(pi_state, NULL);
            }
        }
        ttosEnableTaskDispatchWithLock();
        sched_lock_held = false;
        futex_pi_state_put(pi_state);
        goto retry;
    }

    ttosEnableTaskDispatchWithLock();
    sched_lock_held = false;

    futex_pi_state_inc_waiters(pi_state);
    ret = rt_mutex_lock_timeout(pi_state->pi, ticks);
    futex_pi_state_dec_waiters(pi_state);

    if (ret < 0)
    {
        futex_pi_state_put(pi_state);
        return ret;
    }
    if (ret != 0)
    {
        futex_pi_state_put(pi_state);
        return -ttos_ret_to_errno(ret);
    }

    futex_pi_state_set_owner(pi_state, cur_tcb);

    while (1)
    {
        word = atomic32_read((atomic32_t *)uaddr);
        int new_word = (word & (FUTEX_WAITERS | FUTEX_OWNER_DIED)) | cur_tcb->tid;
        ret = _futex_cmpxchg_value(&cword, uaddr, word, new_word);
        if (ret == 0)
        {
            break;
        }
        if (ret != -EAGAIN)
        {
            futex_pi_state_put(pi_state);
            return ret;
        }
    }

    if (pi_state->owner_died || (word & FUTEX_OWNER_DIED))
    {
        pi_state->owner_died = 1;
        futex_pi_state_put(pi_state);
        return -EOWNERDEAD;
    }

    futex_pi_state_put(pi_state);
    return 0;

out_unlock_put:
    if (sched_lock_held)
    {
        ttosEnableTaskDispatchWithLock();
    }
    futex_pi_state_put(pi_state);
    return ret;
}

static int futex_unlock_pi(struct futex_key *key, uint32_t *uaddr, int op_flags)
{
    int word;
    int cword;
    int tid;
    int ret;
    TASK_ID next = NULL;
    struct futex_pi_state *pi_state;
    T_TTOS_TaskControlBlock *cur_tcb = ttosGetRunningTask();

    (void)op_flags;

    if (!user_access_check(uaddr, sizeof(*uaddr), UACCESS_RW))
    {
        return -EFAULT;
    }

    pi_state = futex_pi_state_get(key, false);
    if (!pi_state)
    {
        word = atomic32_read((atomic32_t *)uaddr);
        tid = word & FUTEX_TID_MASK;
        if (tid != cur_tcb->tid)
        {
            return -EPERM;
        }

        while (1)
        {
            int new_word = (word & FUTEX_WAITERS) & ~FUTEX_TID_MASK;
            ret = _futex_cmpxchg_value(&cword, uaddr, word, new_word);
            if (ret == 0)
            {
                return 0;
            }
            if (ret != -EAGAIN)
            {
                return ret;
            }
            word = atomic32_read((atomic32_t *)uaddr);
            tid = word & FUTEX_TID_MASK;
            if (tid != cur_tcb->tid)
            {
                return -EPERM;
            }
        }
    }

    if (pi_state->pi->owner != cur_tcb)
    {
        futex_pi_state_put(pi_state);
        return -EPERM;
    }

    ret = rt_mutex_unlock_prepare(pi_state->pi, &next);
    if (ret)
    {
        futex_pi_state_put(pi_state);
        return ret;
    }

    if (next)
    {
        futex_pi_state_set_owner(pi_state, next);
        futex_pi_state_dec_waiters(pi_state);
    }
    else
    {
        futex_pi_state_set_owner(pi_state, NULL);
    }
    pi_state->owner_died = 0;

    while (1)
    {
        word = atomic32_read((atomic32_t *)uaddr);
        int new_word;
        if (next)
        {
            new_word = (word & FUTEX_WAITERS) | next->tid;
        }
        else
        {
            new_word = word & FUTEX_WAITERS;
        }
        new_word &= ~FUTEX_OWNER_DIED;

        ret = _futex_cmpxchg_value(&cword, uaddr, word, new_word);
        if (ret == 0)
        {
            break;
        }
        if (ret != -EAGAIN)
        {
            futex_pi_state_put(pi_state);
            return ret;
        }
    }

    rt_mutex_unlock_finish(pi_state->pi, next);
    futex_pi_state_put(pi_state);
    return 0;
}

static int futex_wait_requeue_pi(struct futex_key *key1, struct futex_key *key2, uint32_t *uaddr,
                                 uint32_t *uaddr2, uint32_t val, const struct timespec *timeout,
                                 int op_flags)
{
    struct futex_bucket *bucket;
    struct futex_q *q;
    unsigned long flags;
    u32 uval;
    bool has_timeout = false;
    bool timeout_expired;
    bool use_realtime_clock = (op_flags & FUTEX_CLOCK_REALTIME) != 0;

    (void)key2;
    (void)uaddr;
    (void)uaddr2;

    if (!user_access_check((void *)(uintptr_t)key1->uaddr, sizeof(int), UACCESS_R))
    {
        return -EFAULT;
    }
    if (!user_access_check((void *)(uintptr_t)key2->uaddr, sizeof(int), UACCESS_R))
    {
        return -EFAULT;
    }

    if (timeout != NULL)
    {
        if (timeout->tv_sec < 0 || timeout->tv_nsec < 0 || timeout->tv_nsec >= 1000000000)
        {
            return -EINVAL;
        }
        has_timeout = true;
    }
    timeout_expired = futex_timeout_expired(timeout, true, use_realtime_clock);

    q = &current->futex;
    bucket = &futex_table[futex_hash_bucket_get(key1)];

    ttosDisableTaskDispatchWithLock();

    spin_lock_irqsave(&bucket->lock, flags);
    uval = atomic32_read((atomic32_t *)(uintptr_t)key1->uaddr);
    if (uval != val)
    {
        spin_unlock_irqrestore(&bucket->lock, flags);
        ttosEnableTaskDispatchWithLock();
        return -EAGAIN;
    }
    if (timeout_expired)
    {
        spin_unlock_irqrestore(&bucket->lock, flags);
        ttosEnableTaskDispatchWithLock();
        return -ETIMEDOUT;
    }

    q->bitset = FUTEX_BITSET_MATCH_ANY;
    q->pcb = current;
    q->key = *key1;
    q->pi_state = NULL;
    q->flags = FUTEX_Q_FLAG_WAIT_REQUEUE_PI;
    q->pcb->taskControlId->wait.returnCode = 0;

    INIT_LIST_HEAD(&q->list);

    list_add_head(&q->list, &bucket->chain);
    (void)ttosClearTaskReady(q->pcb->taskControlId);
    q->pcb->taskControlId->state = TTOS_TASK_INTERRUPTIBLE_BY_SIGNAL | TTOS_TASK_WAITING_FOR_FUTEX;
    // spin_unlock_irqrestore(&bucket->lock, flags);
    spin_unlock(&bucket->lock);

    if (has_timeout)
    {
        q->pcb->taskControlId->waitedTicks =
            futex_timeout_to_tick(timeout, true, use_realtime_clock);
        (void)ttosInsertWaitedTask(q->pcb->taskControlId);
    }

    ttosEnableTaskDispatchWithLock();

    ttos_int_unlock(flags);

    if (q->pcb->taskControlId->wait.returnCode)
    {
        if (q->pi_state)
        {
            rt_mutex_waiter_abort(q->pcb->taskControlId);
            futex_pi_state_dec_waiters(q->pi_state);
            futex_pi_state_put(q->pi_state);
            q->pi_state = NULL;
        }
        spin_lock_irqsave(&bucket->lock, flags);
        list_del_init(&q->list);
        spin_unlock_irqrestore(&bucket->lock, flags);
        return -q->pcb->taskControlId->wait.returnCode;
    }

    if (!q->pi_state)
    {
        return -EAGAIN;
    }

    if (q->pi_state)
    {
        futex_pi_state_put(q->pi_state);
        q->pi_state = NULL;
    }

    return 0;
}

static int futex_requeue_pi(struct futex_key *key1, struct futex_key *key2, int num_wake,
                            int num_requeue)
{
    int woken_cnt = 0;
    int req_cnt = 0;
    struct futex_q *q;
    struct futex_q *tmp;
    unsigned long flags;
    struct futex_bucket *bucket1, *bucket2;
    struct futex_pi_state *pi_state;
    TASK_ID next = NULL;
    TASK_ID owner_task = NULL;
    uint32_t *uaddr2;
    int word2;
    int cword;
    int tid2;
    LIST_HEAD(requeue_list);

    if (key1->pid == key2->pid && key1->uaddr == key2->uaddr)
    {
        return -EINVAL;
    }

    pi_state = futex_pi_state_get(key2, true);
    if (!pi_state)
    {
        return -ENOMEM;
    }

    uaddr2 = (uint32_t *)(uintptr_t)key2->uaddr;
    word2 = atomic32_read((atomic32_t *)uaddr2);
    tid2 = word2 & FUTEX_TID_MASK;
    if (!(word2 & FUTEX_WAITERS))
    {
        int nword = word2 | FUTEX_WAITERS;
        if (_futex_cmpxchg_value(&cword, uaddr2, word2, nword) == 0)
        {
            word2 = nword;
        }
    }

    if (word2 & FUTEX_OWNER_DIED)
    {
        pi_state->owner_died = 1;
    }

    if (tid2)
    {
        owner_task = task_get_by_tid(tid2);
        if (owner_task && futex_owner_task_sane(owner_task, tid2))
        {
            if (pi_state->owner != owner_task)
            {
                if (pi_state->owner && !futex_owner_task_sane(pi_state->owner, pi_state->owner_tid))
                {
                    INIT_LIST_HEAD(&pi_state->pi->owner_node);
                    pi_state->pi->owner = NULL;
                    futex_pi_state_set_owner(pi_state, NULL);
                }

                if (pi_state->owner)
                {
                    unsigned long oflags1;
                    int nret;
                    spin_lock_irqsave(&pi_state->owner->held_rt_mutexs_lock, oflags1);
                    nret = futex_owner_node_detach_safe(pi_state);
                    spin_unlock_irqrestore(&pi_state->owner->held_rt_mutexs_lock, oflags1);
                    if (nret)
                    {
                        futex_pi_state_put(pi_state);
                        return nret;
                    }
                }

                unsigned long oflags2;
                int nret;
                spin_lock_irqsave(&owner_task->held_rt_mutexs_lock, oflags2);
                nret = futex_owner_node_attach_safe(pi_state, owner_task);
                spin_unlock_irqrestore(&owner_task->held_rt_mutexs_lock, oflags2);
                if (nret)
                {
                    futex_pi_state_put(pi_state);
                    return nret;
                }

                pi_state->pi->owner = owner_task;
                futex_pi_state_set_owner(pi_state, owner_task);
            }
        }
    }

    bucket1 = &futex_table[futex_hash_bucket_get(key1)];
    bucket2 = &futex_table[futex_hash_bucket_get(key2)];

    ttosDisableTaskDispatchWithLock();

    futex_bucket_pair_lock(bucket1, bucket2, &flags);

    list_for_each_entry_safe(q, tmp, &bucket1->chain, list)
    {
        if (q->key.pid == key1->pid && q->key.uaddr == key1->uaddr &&
            !(q->flags & FUTEX_Q_FLAG_WAIT_REQUEUE_PI))
        {
            list_del_init(&q->list);
            q->pcb->taskControlId->state &=
                ~(TTOS_TASK_INTERRUPTIBLE_BY_SIGNAL | TTOS_TASK_WAITING_FOR_FUTEX);
            if (q->pcb->taskControlId->objCore.objectNode.next != NULL)
            {
                ttosExactWaitedTask(q->pcb->taskControlId);
            }
            ttosClearTaskWaiting(q->pcb->taskControlId);
            woken_cnt++;
            if (num_wake > 0 && woken_cnt >= num_wake)
            {
                break;
            }
        }
    }

    list_for_each_entry_safe(q, tmp, &bucket1->chain, list)
    {
        if (q->key.pid == key1->pid && q->key.uaddr == key1->uaddr &&
            (q->flags & FUTEX_Q_FLAG_WAIT_REQUEUE_PI))
        {
            list_del_init(&q->list);
            q->key = *key2;
            q->pi_state = pi_state;
            q->pcb->taskControlId->state &= ~TTOS_TASK_WAITING_FOR_FUTEX;
            q->pcb->taskControlId->state |= TTOS_TASK_WAITING_FOR_MUTEX;
            list_add_tail(&q->list, &requeue_list);

            req_cnt++;
            if (num_requeue > 0 && req_cnt >= num_requeue)
            {
                break;
            }
        }
    }
    futex_bucket_pair_unlock(bucket1, bucket2, flags);

    ttosEnableTaskDispatchWithLock();

    list_for_each_entry_safe(q, tmp, &requeue_list, list)
    {
        futex_pi_state_inc_waiters(pi_state);
        if (rt_mutex_enqueue_waiter(pi_state->pi, q->pcb->taskControlId))
        {
            futex_pi_state_dec_waiters(pi_state);
            q->pi_state = NULL;
        }
        list_del_init(&q->list);
    }

    if (pi_state->pi->owner == NULL)
    {
        rt_mutex_take_top_waiter(pi_state->pi, &next);
        if (next)
        {
            futex_pi_state_set_owner(pi_state, next);
            futex_pi_state_dec_waiters(pi_state);
            uint32_t *uaddr2 = (uint32_t *)(uintptr_t)key2->uaddr;
            int word = atomic32_read((atomic32_t *)uaddr2);
            int cword;
            int new_word = (word & FUTEX_WAITERS) | next->tid;
            if (pi_state->owner_died)
            {
                new_word |= FUTEX_OWNER_DIED;
            }
            (void)_futex_cmpxchg_value(&cword, uaddr2, word, new_word);
            rt_mutex_wake_owner(pi_state->pi, next);
        }
    }

    futex_pi_state_put(pi_state);
    return woken_cnt + req_cnt;
}

static void futex_robust_handle_uaddr(T_TTOS_TaskControlBlock *task, uint32_t __user *uaddr)
{
    int word;
    int cword;
    struct futex_key key;
    struct futex_key shared_key;
    struct futex_pi_state *pi_state;
    TASK_ID next = NULL;
    bool has_process_key = false;

    if (!user_access_check(uaddr, sizeof(*uaddr), UACCESS_RW))
    {
        return;
    }

    word = atomic32_read((atomic32_t *)uaddr);
    if ((word & FUTEX_TID_MASK) != (task->tid & FUTEX_TID_MASK))
    {
        return;
    }

    key.pid = 0;
    key.uaddr = (uintptr_t)uaddr;
    shared_key = key;

    if (task->ppcb)
    {
        key.pid = get_process_pid((pcb_t)task->ppcb);
        has_process_key = (key.pid != 0);
    }

    pi_state = futex_pi_state_get(&key, false);
    if (!pi_state && has_process_key)
    {
        pi_state = futex_pi_state_get(&shared_key, false);
    }
    if (pi_state && pi_state->owner == task)
    {
        pi_state->owner_died = 1;
        (void)rt_mutex_unlock_prepare(pi_state->pi, &next);
        if (next)
        {
            futex_pi_state_dec_waiters(pi_state);
            futex_pi_state_set_owner(pi_state, next);
        }
        else
        {
            futex_pi_state_set_owner(pi_state, NULL);
        }

        word = atomic32_read((atomic32_t *)uaddr);
        int new_word;
        if (next)
        {
            new_word = (word & FUTEX_WAITERS) | FUTEX_OWNER_DIED | next->tid;
        }
        else
        {
            new_word = (word & FUTEX_WAITERS) | FUTEX_OWNER_DIED;
        }
        (void)_futex_cmpxchg_value(&cword, uaddr, word, new_word);

        rt_mutex_unlock_finish(pi_state->pi, next);
        futex_pi_state_put(pi_state);
        return;
    }

    if (pi_state)
    {
        pi_state->owner_died = 1;
        futex_pi_state_put(pi_state);
    }

    word = atomic32_read((atomic32_t *)uaddr);
    int new_word = (word & FUTEX_WAITERS) | FUTEX_OWNER_DIED;
    (void)_futex_cmpxchg_value(&cword, uaddr, word, new_word);

    futex_wake(&key, 1, FUTEX_BITSET_MATCH_ANY);
    if (has_process_key)
    {
        futex_wake(&shared_key, 1, FUTEX_BITSET_MATCH_ANY);
    }
}

void futex_robust_list_exit(T_TTOS_TaskControlBlock *task)
{
    struct robust_list_head head;
    struct robust_list *curr;
    unsigned int count = 0;

    if (!task || !task->robust_list_head)
    {
        return;
    }

    if (task->robust_list_len < sizeof(head))
    {
        return;
    }

    if (copy_from_user(&head, task->robust_list_head, sizeof(head)) < 0)
    {
        return;
    }

    curr = head.list.next;
    while (curr && curr != (struct robust_list *)task->robust_list_head &&
           count < FUTEX_ROBUST_LIST_LIMIT)
    {
        struct robust_list node;
        uint32_t __user *uaddr = (uint32_t __user *)((uintptr_t)curr + head.futex_offset);
        futex_robust_handle_uaddr(task, uaddr);

        if (copy_from_user(&node, curr, sizeof(node)) < 0)
        {
            break;
        }
        curr = node.next;
        count++;
    }

    if (head.list_op_pending)
    {
        uint32_t __user *uaddr =
            (uint32_t __user *)((uintptr_t)head.list_op_pending + head.futex_offset);
        futex_robust_handle_uaddr(task, uaddr);
    }
}

/**
 * @brief 系统调用实现：快速用户空间互斥锁操作。
 *
 * 该函数实现了一个系统调用，提供用户空间的快速互斥锁机制。
 * 用于进程间同步。
 *
 * @param[in] uaddr 用户空间的futex地址
 * @param[in] op futex操作码
 * @param[in] val 操作相关的值
 * @param[in] timeout 超时时间结构体
 * @param[in] uaddr2 第二个futex地址(用于某些操作)
 * @param[in] val3 额外的值(用于某些操作)
 * @return 成功时返回值依赖于操作，失败时返回负值错误码。
 * @retval >=0 成功(具体值依赖于操作)。
 * @retval -EAGAIN 操作无法立即完成。
 * @retval -EINTR 等待被信号中断。
 * @retval -EINVAL 参数无效。
 * @retval -ETIMEDOUT 操作超时。
 *
 * @note 1. 主要用于实现用户空间同步原语。
 *       2. 结合了用户空间的原子操作。
 *       3. 避免了不必要的内核态切换。
 *       4. 支持多种同步操作。
 */
DEFINE_SYSCALL(futex, (uint32_t __user * uaddr, int op, uint32_t val, struct timespec __user *utime,
                       uint32_t __user *uaddr2, uint32_t val3))
{
    struct futex_key key1, key2;

    int op_type = op & ~FUTEX_FLAGS;
    int op_flags = op & FUTEX_FLAGS;

    int ret = 0;

    if (op_flags & FUTEX_PRIVATE_FLAG)
    {
        key1.pid = key2.pid = get_process_pid(current);
    }
    else
    {
        key1.pid = key2.pid = 0;
    }
    key1.uaddr = (uintptr_t)uaddr;
    key2.uaddr = (uintptr_t)uaddr2;

    if (key1.uaddr & 0x3)
    {
        return -EINVAL;
    }

    if ((op_flags & FUTEX_CLOCK_REALTIME) && op_type != FUTEX_WAIT &&
        op_type != FUTEX_WAIT_BITSET && op_type != FUTEX_WAIT_REQUEUE_PI)
    {
        return -ENOSYS;
    }

    switch (op_type)
    {
    case FUTEX_WAIT:
        val3 = FUTEX_BITSET_MATCH_ANY;
    case FUTEX_WAIT_BITSET:
    {
        struct timespec ktime;
#ifdef SYS_futex_time64
        if (utime != NULL)
        {
            int r = copy_from_user(&ktime.tv_sec, &(((long *)utime)[0]), sizeof(long));
            if (r < 0)
            {
                return r;
            }

            r = copy_from_user(&ktime.tv_nsec, &(((long *)utime)[1]), sizeof(long));
            if (r < 0)
            {
                return r;
            }
        }
#else
        if (utime != NULL)
        {
            ret = copy_from_user(&ktime, utime, sizeof(ktime));
            if (ret < 0)
            {
                return ret;
            }
        }
#endif
        ret = futex_wait(&key1, val, utime == NULL ? NULL : &ktime, val3, op_flags,
                         FUTEX_WAIT_BITSET == op_type);
    }
    break;
    case FUTEX_WAKE:
        val3 = FUTEX_BITSET_MATCH_ANY;
        fallthrough;
    case FUTEX_WAKE_BITSET:
        ret = futex_wake(&key1, val, val3);
        break;
    case FUTEX_FD:
        KLOG_E("futex fd is not implemented remove by linux 2.6.26");
        break;
    case FUTEX_REQUEUE:
        ret = futex_requeue(&key1, &key2, val, (long)utime);
        break;
    case FUTEX_CMP_REQUEUE:
        if (key2.uaddr & 0x3)
        {
            return -EINVAL;
        }

        if (*uaddr == val3)
        {
            ret = futex_requeue(&key1, &key2, val, (long)utime);
        }
        else
        {
            ret = -EAGAIN;
        }
        break;
    case FUTEX_WAKE_OP:
        KLOG_D("futex wake op");
        break;
    case FUTEX_LOCK_PI:
    {
        struct timespec ktime;
#ifdef SYS_futex_time64
        if (utime != NULL)
        {
            int r = copy_from_user(&ktime.tv_sec, &(((long *)utime)[0]), sizeof(long));
            if (r < 0)
            {
                return r;
            }

            r = copy_from_user(&ktime.tv_nsec, &(((long *)utime)[1]), sizeof(long));
            if (r < 0)
            {
                return r;
            }
        }
#else
        if (utime != NULL)
        {
            ret = copy_from_user(&ktime, utime, sizeof(ktime));
            if (ret < 0)
            {
                return ret;
            }
        }
#endif
        ret = futex_lock_pi(&key1, uaddr, utime == NULL ? NULL : &ktime, op_flags, false);
    }
    break;
    case FUTEX_UNLOCK_PI:
        ret = futex_unlock_pi(&key1, uaddr, op_flags);
        break;
    case FUTEX_TRYLOCK_PI:
        ret = futex_lock_pi(&key1, uaddr, NULL, op_flags, true);
        break;
    case FUTEX_WAIT_REQUEUE_PI:
    {
        struct timespec ktime;
        if (key2.uaddr & 0x3)
        {
            return -EINVAL;
        }
#ifdef SYS_futex_time64
        if (utime != NULL)
        {
            int r = copy_from_user(&ktime.tv_sec, &(((long *)utime)[0]), sizeof(long));
            if (r < 0)
            {
                return r;
            }

            r = copy_from_user(&ktime.tv_nsec, &(((long *)utime)[1]), sizeof(long));
            if (r < 0)
            {
                return r;
            }
        }
#else
        if (utime != NULL)
        {
            ret = copy_from_user(&ktime, utime, sizeof(ktime));
            if (ret < 0)
            {
                return ret;
            }
        }
#endif
        ret = futex_wait_requeue_pi(&key1, &key2, uaddr, uaddr2, val, utime == NULL ? NULL : &ktime,
                                    op_flags);
    }
    break;
    case FUTEX_CMP_REQUEUE_PI:
        if (key2.uaddr & 0x3)
        {
            return -EINVAL;
        }
        if (val != 1)
        {
            return -EINVAL;
        }
        if (*uaddr == val3)
        {
            ret = futex_requeue_pi(&key1, &key2, val, (long)utime);
        }
        else
        {
            ret = -EAGAIN;
        }
        break;
    default:
        KLOG_E("futex op=0x%x optype 0x%x is not implemented", op, op_type);
        break;
    }

    return ret;
}
