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

/************************头 文 件******************************/
#include <atomic.h>
#include <barrier.h>
#include <cpuid.h>
#include <spinlock.h>
#include <stdio.h>
#include <system/compiler.h>
#include <ttos.h>
#include <stdbool.h>
#include <util.h>

#define KLOG_TAG "Kernel"
#include <klog.h>

/************************宏 定 义******************************/
#define SPINLOCK_TIMEOUT_MS 1000

/************************类型定义******************************/

/************************外部声明******************************/
void backtrace_r(const char *cookie, uintptr_t frame_address);

/************************前向声明******************************/

/************************模块变量******************************/

/************************全局变量******************************/
#if defined(TTOS_SPINLOCK_WITH_RECURSION)
DEFINE_SPINLOCK(ttosKernelLockVar);
#else
DEFINE_KERNEL_SPINLOCK(ttosKernelLockVar);
#endif

/************************函数实现******************************/

#if defined(TTOS_SPINLOCK_WITH_RECURSION)

DEFINE_SPINLOCK(deadlock_spinlock);

static void deadlock_check(T_UDWORD start_count)
{
    T_UDWORD now_count = TTOS_GetCurrentSystemCount();
    T_UDWORD wait_ms = TTOS_GetCurrentSystemSubTime(start_count, now_count, TTOS_MS_UNIT);

    if (wait_ms > SPINLOCK_TIMEOUT_MS)
    {
        irq_flags_t flags;

        spin_lock_irqsave(&deadlock_spinlock, flags);

        printk("spinlock may deadlock!\n");
        uintptr_t fp = (uintptr_t)__builtin_frame_address(0);
        if (fp)
        {
            backtrace_r("deadlock", fp);
        }

        printk("while(1) at %s:%d\n", __FILE__, __LINE__);
        spin_unlock_irqrestore(&deadlock_spinlock, flags);
        while (1)
            ;
    }
}

/*
 * @brief:
 *     获取自旋锁
 * @param[in] lock 自旋锁对象
 * @retval 无
 */
void kernel_spin_lock(ttos_spinlock_t *lock)
{
    s32 cpu_id;
    int ticket;
    size_t flags = 0;
    uint32_t compete = 0;
    u64 start_count = 0;
    lock->compete_duration = 0;

    ttos_int_lock(flags);

    /* 获取当前CPU号 */
    cpu_id = cpuid_get();

    /* 如果自旋锁嵌套获取则只增加嵌套计数 */
    if (likely(lock->cpu_id != cpu_id))
    {
        start_count = TTOS_GetCurrentSystemCount();

        /* 获取当前票号 */
        ticket = atomic_inc_return(&lock->ticket);

        /* 自旋等待服务票号是当前票号 */
        while (ticket != atomic_read(&lock->ticked_served))
        {
            deadlock_check(start_count);
            compete = 1;
        }

        /* 设置自旋锁拥有者CPU */
        lock->cpu_id = cpu_id;
    }

    /* 内存栅栏，同步以上操作 */
    smp_mb();

    /* 增加自旋锁嵌套计数 */
    lock->nest_count++;

    if (compete)
        lock->compete_duration = TTOS_GetCurrentSystemCount() - start_count;
    ttos_int_unlock(flags);
}

/*
 * @brief:
 *     尝试获取自旋锁
 * @param[in] lock 自旋锁对象
 * @retval true 获取成功
 *         false 获取失败
 */
bool kernel_spin_trylock(ttos_spinlock_t *lock)
{
    s32 cpu_id;
    int ticket;
    size_t flags = 0;

    ttos_int_lock(flags);

    /* 获取当前CPU号 */
    cpu_id = cpuid_get();

    /* 如果自旋锁嵌套获取则只增加嵌套计数 */
    if (likely(lock->cpu_id != cpu_id))
    {
        /* 获取当前票号 */
        // ticket = atomic_inc_return(&lock->ticket);
        ticket = atomic_read(&lock->ticket);

        if ((ticket + 1) != atomic_read(&lock->ticked_served))
        {
            ttos_int_unlock(flags);
            return false;
        }

        if (!atomic_cas(&lock->ticket, ticket, ticket + 1))
        {
            ttos_int_unlock(flags);
            return false;
        }

        /* 设置自旋锁拥有者CPU */
        lock->cpu_id = cpu_id;
    }

    /* 内存栅栏，同步以上操作 */
    smp_mb();

    /* 增加自旋锁嵌套计数 */
    lock->nest_count++;

    ttos_int_unlock(flags);

    return true;
}

/*
 * @brief:
 *     释放自旋锁
 * @param[in] lock 自旋锁对象
 * @retval 无
 */
void kernel_spin_unlock(ttos_spinlock_t *lock)
{
    u32 cpu_id;
    size_t flags = 0;

    ttos_int_lock(flags);

    /* 获取当前CPU号 */
    cpu_id = cpuid_get();

    /* 拥有者与释放者必须是同一CPU */
    if (lock->cpu_id == cpu_id)
    {
        /* 判断内核锁是否嵌套 */
        if (likely(--lock->nest_count == 0))
        {
            /* 设置当前获取锁的CPU */
            lock->cpu_id = TTOS_CPU_NONE;

            /* 内存栅栏，同步以上操作 */
            smp_mb();

            /* 增加服务票号 */
            atomic_inc(&lock->ticked_served);
        }
    }
    else
    {
        /* 调用内核故障打印接口 */
        KLOG_E("kernel_spin_unlock panic cpu_id:%d", cpu_id);
        uintptr_t fp = (uintptr_t)__builtin_frame_address(0);
        if (fp)
        {
            backtrace_r("deadlock", fp);
        }
    }

    ttos_int_unlock(flags);
}
/**
 * @brief 检查自旋锁是否已经被锁定
 *
 * @param[in] lock 自旋锁对象
 * @return true 表示锁已被锁定，false 表示锁未锁定
 */
bool kernel_spin_is_locked(const ttos_spinlock_t *lock)
{
    return lock->nest_count;
}

#else

DEFINE_SPINLOCK(deadlock_spinlock);

static void deadlock_check(T_UDWORD start_count)
{
    T_UDWORD now_count = TTOS_GetCurrentSystemCount();
    T_UDWORD wait_ms = TTOS_GetCurrentSystemSubTime(start_count, now_count, TTOS_MS_UNIT);

    if (wait_ms > SPINLOCK_TIMEOUT_MS)
    {
        irq_flags_t flags;

        spin_lock_irqsave(&deadlock_spinlock, flags);

        printk("spinlock may deadlock!\n");
        uintptr_t fp = (uintptr_t)__builtin_frame_address(0);
        if (fp)
        {
            backtrace_r("deadlock", fp);
        }

        printk("while(1) at %s:%d\n", __FILE__, __LINE__);
        spin_unlock_irqrestore(&deadlock_spinlock, flags);
        while (1)
            ;
    }
}

void kernel_spin_lock_with_recursion(ttos_kernel_spinlock_t *lock)
{
    s32 cpu_id;
    int ticket;
    size_t flags = 0;
    uint32_t compete = 0;
    u64 start_count = 0;
    lock->compete_duration = 0;

    ttos_int_lock(flags);

    /* 获取当前CPU号 */
    cpu_id = cpuid_get();

    /* 如果自旋锁嵌套获取则只增加嵌套计数 */
    if (likely(lock->cpu_id != cpu_id))
    {
        start_count = TTOS_GetCurrentSystemCount();

        /* 获取当前票号 */
        ticket = atomic_inc_return(&lock->ticket);

        /* 自旋等待服务票号是当前票号 */
        while (ticket != atomic_read(&lock->ticked_served))
        {
            deadlock_check(start_count);
            compete = 1;
        }

        /* 设置自旋锁拥有者CPU */
        lock->cpu_id = cpu_id;
    }

    /* 内存栅栏，同步以上操作 */
    smp_mb();

    /* 增加自旋锁嵌套计数 */
    lock->nest_count++;

    if (compete)
        lock->compete_duration = TTOS_GetCurrentSystemCount() - start_count;
    ttos_int_unlock(flags);
}

bool kernel_spin_trylock_with_recursion(ttos_kernel_spinlock_t *lock)
{
    s32 cpu_id;
    int ticket;
    size_t flags = 0;

    ttos_int_lock(flags);

    /* 获取当前CPU号 */
    cpu_id = cpuid_get();

    /* 如果自旋锁嵌套获取则只增加嵌套计数 */
    if (likely(lock->cpu_id != cpu_id))
    {
        /* 获取当前票号 */
        ticket = atomic_read(&lock->ticket);

        if ((ticket + 1) != atomic_read(&lock->ticked_served))
        {
            ttos_int_unlock(flags);
            return false;
        }

        if (!atomic_cas(&lock->ticket, ticket, ticket + 1))
        {
            ttos_int_unlock(flags);
            return false;
        }

        /* 设置自旋锁拥有者CPU */
        lock->cpu_id = cpu_id;
    }

    /* 内存栅栏，同步以上操作 */
    smp_mb();

    /* 增加自旋锁嵌套计数 */
    lock->nest_count++;

    ttos_int_unlock(flags);

    return true;
}

void kernel_spin_unlock_with_recursion(ttos_kernel_spinlock_t *lock)
{
    u32 cpu_id;
    size_t flags = 0;

    ttos_int_lock(flags);

    /* 获取当前CPU号 */
    cpu_id = cpuid_get();

    /* 拥有者与释放者必须是同一CPU */
    if (lock->cpu_id == cpu_id)
    {
        /* 判断内核锁是否嵌套 */
        if (likely(--lock->nest_count == 0))
        {
            /* 设置当前获取锁的CPU */
            lock->cpu_id = TTOS_KERNEL_SPINLOCK_CPU_NONE;

            /* 内存栅栏，同步以上操作 */
            smp_mb();

            /* 增加服务票号 */
            atomic_inc(&lock->ticked_served);
        }
    }
    else
    {
        /* 调用内核故障打印接口 */
        KLOG_E("kernel_spin_unlock_with_recursion panic cpu_id:%d", cpu_id);
        uintptr_t fp = (uintptr_t)__builtin_frame_address(0);
        if (fp)
        {
            backtrace_r("deadlock", fp);
        }
    }

    ttos_int_unlock(flags);
}

void kernel_spin_lock(ttos_spinlock_t *lock)
{
    TTOS_DisablePreempt();
    u32 val = atomic32_fetch_add((atomic32_t *)lock, 1 << TICKET_SHIFT);

    u16 ticket = val >> TICKET_SHIFT;
    if (ticket != (u16)val) {
        /* 有竞争，自旋等待 */
        for (;;) {
            if (ticket == READ_ONCE(lock->owner)) {
                break;
            }
        }
    }

    /*
     * Acquire barrier: 确保所有后续的内存操作不会被重排序到
     * 获取锁之前。无论是否经过自旋等待都需要这个屏障。
     */
    smp_mb();
}



void kernel_spin_unlock(ttos_spinlock_t *lock)
{
    u16 new_owner = lock->owner + 1;
    smp_store_release(&lock->owner, new_owner);
    TTOS_EnablePreempt();
}

/**
 * @brief 检查自旋锁是否已经被锁定
 *
 * @param[in] lock 自旋锁对象
 * @return true 表示锁已被锁定，false 表示锁未锁定
 */
bool kernel_spin_is_locked(const ttos_spinlock_t *lock)
{
    u32 v = READ_ONCE(lock->slock);
    u16 owner = (u16)v;
    u16 next  = (u16)(v >> TICKET_SHIFT);
    return owner != next;
}

#endif /* TTOS_SPINLOCK_WITH_RECURSION */
