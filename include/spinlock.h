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

#ifndef __SNK_SPINLOCK_H__
#define __SNK_SPINLOCK_H__

#include <atomic.h>
#include <barrier.h>
#include <system/types.h>
#include <trace.h>
#include <ttos_arch.h>

void TTOS_DisablePreempt(void);
void TTOS_EnablePreempt(void);

#if defined(TTOS_SPINLOCK_WITH_RECURSION)

#if defined(CONFIG_SMP)

#define TTOS_CPU_NONE (-1)

#define INIT_SPIN_LOCK(_lock)                                                                      \
    do                                                                                             \
    {                                                                                              \
        atomic_write(&(_lock)->ticket, 0);                                                         \
        atomic_write(&(_lock)->ticked_served, 1);                                                  \
        (_lock)->cpu_id = TTOS_CPU_NONE;                                                           \
        (_lock)->nest_count = 0;                                                                   \
    } while (0)

#define __SPINLOCK_INITIALIZER(_lock)                                                              \
    {.ticket = {0}, .ticked_served = {1}, .cpu_id = TTOS_CPU_NONE, .nest_count = 0}

#else
#define INIT_SPIN_LOCK(v) ((v)->lock = 0)
#define __SPINLOCK_INITIALIZER(_lock) {.lock = 0}
#endif

#define DEFINE_SPINLOCK(_lock) ttos_spinlock_t _lock = __SPINLOCK_INITIALIZER(_lock)
#define DECLARE_SPINLOCK(_lock) ttos_spinlock_t _lock

/************************类型定义******************************/
#if defined(CONFIG_SMP)

struct ttos_spinlock
{
    atomic_t ticket;
    atomic_t ticked_served;
    s32 cpu_id;
    u32 nest_count;
    u32 compete_duration;
};
#else
struct ttos_spinlock
{
    u32 lock;
};
#endif

typedef struct ttos_spinlock ttos_spinlock_t;

#if defined(CONFIG_SMP)

#define spin_lock_init(v) INIT_SPIN_LOCK(v)

#define spin_lock(lock)                                                                            \
    do                                                                                             \
    {                                                                                              \
        TTOS_DisablePreempt();                                                                     \
        kernel_spin_lock(lock);                                                                    \
    } while (0)

#define spin_unlock(lock)                                                                          \
    do                                                                                             \
    {                                                                                              \
        kernel_spin_unlock(lock);                                                                  \
        TTOS_EnablePreempt();                                                                      \
    } while (0)

#define spin_trylock(lock)                                                                         \
    ({                                                                                             \
        TTOS_DisablePreempt();                                                                     \
        bool ret = kernel_spin_trylock(lock);                                                      \
        if (!ret)                                                                                  \
        {                                                                                          \
            TTOS_EnablePreempt();                                                                  \
        }                                                                                          \
        ret;                                                                                       \
    })

#define spin_lock_irqsave(lock, flags)                                                             \
    do                                                                                             \
    {                                                                                              \
        ttos_int_lock((flags));                                                                    \
        kernel_spin_lock(lock);                                                                    \
    } while (0)

__always_inline void spin_unlock_trace(ttos_spinlock_t *lock)
{
current_label:
    void *pc = &&current_label;
    if (lock->compete_duration > SPINLOCK_COMPETE_TICK)
        trace_compete(pc, COMPETE_T_SPINLOCK, lock->compete_duration);
}

#define spin_unlock_irqrestore(lock, flags)                                                        \
    do                                                                                             \
    {                                                                                              \
        kernel_spin_unlock(lock);                                                                  \
        ttos_int_unlock(flags);                                                                    \
        spin_unlock_trace(lock);                                                                   \
    } while (0)

#define spin_trylock_irqsave(lock, flags)                                                          \
    ({                                                                                             \
        ttos_int_lock((flags));                                                                    \
        bool ret = kernel_spin_trylock(lock);                                                      \
        if (!ret)                                                                                  \
        {                                                                                          \
            ttos_int_unlock(flags);                                                                \
        }                                                                                          \
        ret;                                                                                       \
    })

#define spin_lock_irq(lock)                                                                        \
    do                                                                                             \
    {                                                                                              \
        arch_cpu_int_disable();                                                                    \
        TTOS_DisablePreempt();                                                                     \
        kernel_spin_lock(lock);                                                                    \
    } while (0)

#define spin_unlock_irq(lock)                                                                      \
    do                                                                                             \
    {                                                                                              \
        kernel_spin_unlock(lock);                                                                  \
        TTOS_EnablePreempt();                                                                      \
        arch_cpu_int_enable();                                                                     \
    } while (0)

#else

#define spin_lock_init(v) INIT_SPIN_LOCK(v)

#define spin_lock(_lock)                                                                           \
    do                                                                                             \
    {                                                                                              \
        TTOS_DisablePreempt();                                                                     \
        (_lock)->lock = 1;                                                                         \
    } while (0)

#define spin_unlock(_lock)                                                                         \
    do                                                                                             \
    {                                                                                              \
        (_lock)->lock = 0;                                                                         \
        TTOS_EnablePreempt();                                                                      \
    } while (0)

#define spin_lock_irqsave(_lock, flags)                                                            \
    do                                                                                             \
    {                                                                                              \
        ttos_int_lock((flags));                                                                    \
        TTOS_DisablePreempt();                                                                     \
        (_lock)->lock = 1;                                                                         \
    } while (0)

#define spin_unlock_irqrestore(_lock, flags)                                                       \
    do                                                                                             \
    {                                                                                              \
        (_lock)->lock = 0;                                                                         \
        TTOS_EnablePreempt();                                                                      \
        ttos_int_unlock(flags);                                                                    \
    } while (0)
#endif

/************************接口声明******************************/
void kernel_spin_lock(ttos_spinlock_t *lock);
void kernel_spin_unlock(ttos_spinlock_t *lock);
/*
 * @brief:
 *     尝试获取自旋锁
 * @param[in] lock 自旋锁对象
 * @retval true 获取成功
 *         false 获取失败
 */
bool kernel_spin_trylock(ttos_spinlock_t *lock);

#else

/* Ticket Spinlock */

#define TICKET_SHIFT 16

typedef struct ttos_spinlock
{
    union {
        u32 slock;
        struct
        {
            u16 owner;
            u16 next;
        };
    };
} ttos_spinlock_t;

void kernel_spin_lock(ttos_spinlock_t *lock);
void kernel_spin_unlock(ttos_spinlock_t *lock);

#define DEFINE_SPINLOCK(name) ttos_spinlock_t name = {.slock = 0}
#define INIT_SPIN_LOCK(name) spin_lock_init(name)
#define DECLARE_SPINLOCK(name) ttos_spinlock_t name

/*
 * 内核全局锁需要保留可递归语义，避免调度路径同 CPU 重入时死锁。
 * 其他普通自旋锁继续使用新版不可递归实现。
 */
#define TTOS_KERNEL_SPINLOCK_CPU_NONE (-1)

typedef struct ttos_kernel_spinlock
{
    atomic_t ticket;
    atomic_t ticked_served;
    s32 cpu_id;
    u32 nest_count;
    u32 compete_duration;
} ttos_kernel_spinlock_t;

#define __KERNEL_SPINLOCK_INITIALIZER(_lock)                                                       \
    {                                                                                              \
        .ticket = {0},                                                                             \
        .ticked_served = {1},                                                                      \
        .cpu_id = TTOS_KERNEL_SPINLOCK_CPU_NONE,                                                   \
        .nest_count = 0,                                                                           \
        .compete_duration = 0,                                                                     \
    }

#define DEFINE_KERNEL_SPINLOCK(_lock)                                                              \
    ttos_kernel_spinlock_t _lock = __KERNEL_SPINLOCK_INITIALIZER(_lock)
#define DECLARE_KERNEL_SPINLOCK(_lock) ttos_kernel_spinlock_t _lock

void kernel_spin_lock_with_recursion(ttos_kernel_spinlock_t *lock);
void kernel_spin_unlock_with_recursion(ttos_kernel_spinlock_t *lock);
bool kernel_spin_trylock_with_recursion(ttos_kernel_spinlock_t *lock);

static __always_inline void spin_lock_init(ttos_spinlock_t *lock)
{
    lock->slock = 0;
}

#define spin_lock(lock)                                                                            \
    do                                                                                             \
    {                                                                                              \
        TTOS_DisablePreempt();                                                                     \
        kernel_spin_lock(lock);                                                                    \
    } while (0)

#define spin_unlock(lock)                                                                          \
    do                                                                                             \
    {                                                                                              \
        kernel_spin_unlock(lock);                                                                  \
        TTOS_EnablePreempt();                                                                      \
    } while (0)

#define spin_trylock(lock)                                                                         \
    ({                                                                                             \
        TTOS_DisablePreempt();                                                                     \
        bool ret = kernel_spin_trylock(lock);                                                      \
        if (!ret)                                                                                  \
        {                                                                                          \
            TTOS_EnablePreempt();                                                                  \
        }                                                                                          \
        ret;                                                                                       \
    })

#define spin_lock_irqsave(lock, flags)                                                             \
    do                                                                                             \
    {                                                                                              \
        ttos_int_lock((flags));                                                                    \
        kernel_spin_lock(lock);                                                                    \
    } while (0)

#define spin_unlock_irqrestore(lock, flags)                                                        \
    do                                                                                             \
    {                                                                                              \
        kernel_spin_unlock(lock);                                                                  \
        ttos_int_unlock(flags);                                                                    \
        spin_unlock_trace(lock);                                                                   \
    } while (0)

#define spin_trylock_irqsave(lock, flags)                                                          \
    ({                                                                                             \
        ttos_int_lock((flags));                                                                    \
        bool ret = kernel_spin_trylock(lock);                                                      \
        if (!ret)                                                                                  \
        {                                                                                          \
            ttos_int_unlock(flags);                                                                \
        }                                                                                          \
        ret;                                                                                       \
    })

#define spin_lock_irq(lock)                                                                        \
    do                                                                                             \
    {                                                                                              \
        arch_cpu_int_disable();                                                                    \
        TTOS_DisablePreempt();                                                                     \
        kernel_spin_lock(lock);                                                                    \
    } while (0)

#define spin_unlock_irq(lock)                                                                      \
    do                                                                                             \
    {                                                                                              \
        kernel_spin_unlock(lock);                                                                  \
        TTOS_EnablePreempt();                                                                      \
        arch_cpu_int_enable();                                                                     \
    } while (0)

__always_inline void spin_unlock_trace(ttos_spinlock_t *lock)
{
    return;
}

#endif /* TTOS_SPINLOCK_WITH_RECURSION */

/*
 * Read-Write Spinlock
 *
 * 使用单个 32 位计数器实现：
 * - bit 31: 写锁标志位 (RWLOCK_WRITER)
 * - bit 0-30: 读者计数
 *
 * 读锁：原子加1，如果写锁位被设置则自旋等待
 * 写锁：CAS 从0设置为 RWLOCK_WRITER，否则自旋
 */
#define RWLOCK_WRITER 0x80000000U

typedef struct rw_spinlock
{
    atomic32_t lock;
} rw_spinlock_t;

#define DEFINE_RW_SPINLOCK(name) rw_spinlock_t name = {.lock = ATOMIC32_INIT(0)}

static __always_inline void rw_spin_lock_init(rw_spinlock_t *lock)
{
    atomic32_set(&lock->lock, 0);
}

static __always_inline void read_lock(rw_spinlock_t *lock)
{
    TTOS_DisablePreempt();

    for (;;)
    {
        u32 val = atomic32_fetch_add(&lock->lock, 1);
        if (likely(!(val & RWLOCK_WRITER)))
        {
            /* 没有写者，获取读锁成功 */
            break;
        }

        /* 有写者，撤销读者计数并自旋 */
        atomic32_dec(&lock->lock);
        while (atomic32_read(&lock->lock) & RWLOCK_WRITER)
            ;
    }

    smp_mb();
}

static __always_inline void read_unlock(rw_spinlock_t *lock)
{
    smp_mb();
    atomic32_dec(&lock->lock);
    TTOS_EnablePreempt();
}

static __always_inline void write_lock(rw_spinlock_t *lock)
{
    TTOS_DisablePreempt();

    for (;;)
    {
        if (likely(atomic32_cas(&lock->lock, 0, RWLOCK_WRITER)))
        {
            /* 锁空闲，获取写锁成功 */
            break;
        }

        /* 有读者或写者，自旋等待 */
        while (atomic32_read(&lock->lock) != 0)
            ;
    }

    smp_mb();
}

static __always_inline void write_unlock(rw_spinlock_t *lock)
{
    atomic32_set(&lock->lock, 0);
    smp_mb();
    TTOS_EnablePreempt();
}

static __always_inline void read_lock_irq(rw_spinlock_t *lock)
{
    arch_cpu_int_disable();
    read_lock(lock);
}

static __always_inline void read_unlock_irq(rw_spinlock_t *lock)
{
    read_unlock(lock);
    arch_cpu_int_enable();
}

static __always_inline unsigned long read_lock_irqsave(rw_spinlock_t *lock)
{
    unsigned long flags;
    ttos_int_lock(flags);
    read_lock(lock);
    return flags;
}

static __always_inline void read_unlock_irqrestore(rw_spinlock_t *lock, unsigned long flags)
{
    read_unlock(lock);
    ttos_int_unlock(flags);
}

static __always_inline void write_lock_irq(rw_spinlock_t *lock)
{
    arch_cpu_int_disable();
    write_lock(lock);
}

static __always_inline void write_unlock_irq(rw_spinlock_t *lock)
{
    write_unlock(lock);
    arch_cpu_int_enable();
}

static __always_inline unsigned long write_lock_irqsave(rw_spinlock_t *lock)
{
    unsigned long flags;
    ttos_int_lock(flags);
    write_lock(lock);
    return flags;
}

static __always_inline void write_unlock_irqrestore(rw_spinlock_t *lock, unsigned long flags)
{
    write_unlock(lock);
    ttos_int_unlock(flags);
}

bool kernel_spin_is_locked(const ttos_spinlock_t *lock);
static inline bool spin_is_locked(ttos_spinlock_t *lock)
{
    return kernel_spin_is_locked(lock);
}

#endif /* __SNK_SPINLOCK_H__ */
