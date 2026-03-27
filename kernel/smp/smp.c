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

#include <atomic.h>
#include <barrier.h>
#include <cache.h>
#include <driver/cpudev.h>
#include <ipi.h>
#include <kmalloc.h>
#include <list.h>
#include <mpscq.h>
#include <smp.h>
#include <string.h>
#include <time/ktime.h>
#include <ttos.h>
#include <ttosMM.h>
#include <ttos_pic.h>

#define KLOG_TAG "SMP"
#include <klog.h>

#define SMP_CPU_COUNTS CONFIG_MAX_CPUS
#define SMP_CPU_INIT_WAIT_TIME (100 * NSEC_PER_MSEC) // 100ms
#define SMP_CALL_TIMEOUT (1000 * NSEC_PER_MSEC)      // 1000ms
#define SMP_CALL_QUEUE_DEPTH 64

extern void _start_ap(void);

static LIST_HEAD(slave_func_list);
struct smp_func
{
    struct list_node node;
    smp_slave_func_t func;
    void *data;
};

struct smp_call_item
{
    void (*func)(void *);
    void *info;
    atomic_t *pending;
};

static mpscq_t smp_call_queues[SMP_CPU_COUNTS];

/**
 * @brief
 *    SMP函数调用的IPI处理函数
 * @param[in] irq 中断号
 * @param[in] data 私有数据
 * @retval 无
 */
static void smp_call_ipi_handler(u32 irq, void *data)
{
    struct cpudev *cpu = cpu_self_get();
    struct smp_call_item item;
    mpscq_t *queue;
    void *slot;

    if (!cpu || cpu->index >= SMP_CPU_COUNTS)
        return;

    queue = &smp_call_queues[cpu->index];
    if (!queue->data)
        return;

    while (1)
    {
        slot = mpscq_peek(queue, 1);
        if (!slot)
            break;

        memcpy(&item, slot, sizeof(item));
        mpscq_pop(queue, 1);

        if (item.func)
            item.func(item.info);
        if (item.pending)
            atomic_dec(item.pending);
    }
}

static s32 smp_call_function_common(cpu_set_t *cpus, void (*func)(void *), void *info,
                                    atomic_t *pending)
{
    struct smp_call_item item;
    u32 count = 0;
    u32 self_cpu = cpuid_get();
    cpu_set_t target_cpus;
    cpu_set_t ipi_cpus;
    bool self_in_set = false;

    if (!func)
        return (-EINVAL);

    item.func = func;
    item.info = info;
    item.pending = pending;

    /* 处理CPU集合 */
    CPU_ZERO(&target_cpus);
    if (!cpus || CPU_COUNT(cpus) == 0)
        CPU_OR(&target_cpus, &target_cpus, TTOS_CPUSET_ENABLED());
    else
        CPU_OR(&target_cpus, &target_cpus, cpus);
    self_in_set = CPU_ISSET(self_cpu, &target_cpus);

    /* 【优化】如果当前CPU在目标集合中，立即执行（避免IPI开销） */
    if (self_in_set)
        func(info);

    /* 排除自己 */
    CPU_CLR(self_cpu, &target_cpus);
    CPU_ZERO(&ipi_cpus);

    {
        struct cpudev *cpu_iter;
        for_each_present_cpu(cpu_iter)
        {
            if (cpu_iter->index >= SMP_CPU_COUNTS)
                continue;
            if (!CPU_ISSET(cpu_iter->index, &target_cpus))
                continue;
            if (cpu_iter->state != CPU_STATE_RUNNING)
                continue;

            {
                mpscq_t *queue = &smp_call_queues[cpu_iter->index];
                mpscq_token_t tok;
                void *slot = mpscq_alloc(queue, 1, &tok);
                if (!slot)
                {
                    KLOG_W("smp_call enqueue cpu%d failed", cpu_iter->index);
                    continue;
                }

                memset(slot, 0, queue->elem_size);
                memcpy(slot, &item, sizeof(item));
                mpscq_push(queue, tok, 1);
            }
            {
                CPU_SET(cpu_iter->index, &ipi_cpus);
                count++;
            }
        }
    }

    if (pending)
        atomic_set(pending, count);

    if (count == 0)
        return 0;

    /* 发送IPI到其他目标CPU */
    {
        struct cpudev *cpu_iter;
        for_each_present_cpu(cpu_iter)
        {
            if (cpu_iter->state != CPU_STATE_RUNNING)
                continue;
            if (CPU_ISSET(cpu_iter->index, &ipi_cpus))
                ttos_pic_ipi_send(GENERAL_IPI_CALL, cpu_iter->index, 0);
        }
    }

    return 0;
}

/**
 * @brief
 *    启动从核CPU
 * @param[in] cpu 启动的CPUID
 * @retval 0 启动成功
 * @retval EFAULT 启动失败
 */
static s32 smp_cpu_startup(struct cpudev *cpu)
{
    phys_addr_t _start_ap_entry;
    u64 end_time = ktime_get_ns() + SMP_CPU_INIT_WAIT_TIME;
    bool time_out = true;
    int ret;

    /* 获取从核启动入口的物理地址 */
    _start_ap_entry = mm_kernel_v2p((virt_addr_t)_start_ap);
    if (0 == _start_ap_entry)
    {
        KLOG_E("%s: failed to get phys addr for entry point", __func__);
        return (-EFAULT);
    }

    cpu->state = CPU_STATE_START_UP;
    cache_dcache_flush((size_t)cpu, sizeof(struct cpudev));

    /* 启动从核 */
    ret = cpu_on(cpu, _start_ap_entry, cpu->index);
    if (ret != 0)
    {
        cpu->state = CPU_STATE_SHUTDOWN;
        KLOG_E("failed to start CPU%d: %d", cpu->index, ret);
        return ret;
    }

    /* 等待从核启动完成 */
    do
    {
        cache_dcache_invalidate((size_t)cpu, sizeof(struct cpudev));

        if (cpu->state == CPU_STATE_RUNNING)
        {
            time_out = false;
            break;
        }
    } while (ktime_get_ns() < end_time);

    if (time_out)
    {
        KLOG_E("start CPU%d time out!", cpu->index);
        return (-EFAULT);
    }

    return (0);
}

/**
 * @brief
 *    设置cpu启动完成状态，并等待主核启动完成
 * @param[in] cpuid 逻辑CPUID
 * @retval 无
 */
static void smp_init_done(void *data)
{
    struct cpudev *cpu = cpu_self_get();

    if (cpu->index >= SMP_CPU_COUNTS)
    {
        KLOG_E("cpuid[%d] error!", cpu->index);
        return;
    }

    cpu->state = CPU_STATE_RUNNING;
    cache_dcache_clean((size_t)cpu, sizeof(struct cpudev));

    /* 确保内存访问已完成 */
    mb();

    memset(&cpu->utime, 0, sizeof(cpu->utime));
    memset(&cpu->stime, 0, sizeof(cpu->stime));
    memset(&cpu->itime, 0, sizeof(cpu->itime));

    kernel_clock_gettime(CLOCK_MONOTONIC, &cpu->last_enter_sys);
}

void smp_slave_add_func(smp_slave_func_t func, void *data)
{
    struct smp_func *entry;

    entry = malloc(sizeof(*entry));
    entry->func = func;
    entry->data = data;
    list_add_tail(&entry->node, &slave_func_list);
}

void smp_slave_init(struct cpudev *cpu)
{
    struct smp_func *entry;

    list_for_each_entry(entry, &slave_func_list, node)
    {
        entry->func(entry->data);
    }
}

/**
 * @brief
 *    以异步方式在指定CPU集合上执行函数
 * @param[in] cpus 目标CPU集合，NULL表示所有CPU
 * @param[in] func 要执行的函数
 * @param[in] info 函数参数（指针）
 * @retval 0 成功
 * @retval EINVAL 参数无效
 * @retval EIO IPI发送失败
 * @note 如果当前CPU在目标集合中，将立即执行函数（避免IPI开销）
 */
s32 smp_call_function_async(cpu_set_t *cpus, void (*func)(void *), void *info)
{
    return smp_call_function_common(cpus, func, info, NULL);
}

/**
 * @brief
 *    以同步方式在指定CPU集合上执行函数
 * @param[in] cpus 目标CPU集合，NULL表示所有CPU
 * @param[in] func 要执行的函数
 * @param[in] info 函数参数（指针）
 * @retval 0 成功
 * @retval EINVAL 参数无效
 * @retval EIO IPI发送失败或超时
 */
s32 smp_call_function_sync(cpu_set_t *cpus, void (*func)(void *), void *info)
{
    s32 ret;
    u64 end_time;
    atomic_t pending;

    ret = smp_call_function_common(cpus, func, info, &pending);
    if (ret != 0)
        return ret;

    /* 等待所有CPU完成 */
    if (atomic_read(&pending) == 0)
        return 0;

    end_time = ktime_get_ns() + SMP_CALL_TIMEOUT;
    while (atomic_read(&pending) > 0)
    {
        if (ktime_get_ns() >= end_time)
        {
            KLOG_E("smp_call_function_sync timeout");
            return (-EIO);
        }
    }

    return 0;
}

/**
 * @brief
 *    多核初始化，启动所有从核并注册SMP call IPI处理函数
 * @param[in] 无
 * @retval 无
 */
void smp_master_init(void)
{
    struct cpudev *cpu;
    uint32_t irq;

    smp_slave_add_func(smp_init_done, NULL);

    irq = ttos_pic_irq_alloc(NULL, GENERAL_IPI_CALL);
    if (irq)
    {
        ttos_pic_irq_install(irq, smp_call_ipi_handler, NULL, 0, "IPI SMP Call");
        KLOG_I("SMP call IPI handler registered, irq: %d", irq);
    }
    else
    {
        KLOG_E("Failed to allocate IPI for smp_call");
    }

    for_each_present_cpu(cpu)
    {
        if (cpu->index < SMP_CPU_COUNTS)
        {
            if (mpscq_init(&smp_call_queues[cpu->index], SMP_CALL_QUEUE_DEPTH,
                           sizeof(struct smp_call_item)) != 0)
            {
                KLOG_E("smp call queue init failed for cpu%d", cpu->index);
            }
        }

        KLOG_I("cpu index: %d", cpu->index);
        if (cpu->boot_cpu)
            continue;

        smp_cpu_startup(cpu);
    }
}
