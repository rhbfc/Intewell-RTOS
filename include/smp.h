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

#ifndef __KERNEL_SMP_H__
#define __KERNEL_SMP_H__

#include <system/types.h>

struct cpudev;
typedef struct cpu_set_t cpu_set_t;

typedef void (*smp_slave_func_t)(void *data);

#ifdef CONFIG_SMP

void smp_master_init(void);
void smp_slave_init(struct cpudev *cpu);
void smp_slave_add_func(smp_slave_func_t func, void *data);

/**
 * @brief
 *    以异步方式在指定CPU集合上执行函数，立即返回
 * @param[in] cpus 目标CPU集合（NULL或为空表示所有CPU）
 * @param[in] func 要执行的函数指针
 * @param[in] info 函数参数（通过指针传递，调用者保证其生命周期）
 * @retval 0 成功发送IPI
 * @retval EINVAL 参数无效
 * @retval EIO IPI发送失败
 */
s32 smp_call_function_async(cpu_set_t *cpus, void (*func)(void *), void *info);

/**
 * @brief
 *    以同步方式在指定CPU集合上执行函数，等待所有CPU完成
 * @param[in] cpus 目标CPU集合（NULL或为空表示所有CPU）
 * @param[in] func 要执行的函数指针
 * @param[in] info 函数参数（通过指针传递，调用者保证其生命周期）
 * @retval 0 所有CPU成功执行
 * @retval EINVAL 参数无效
 * @retval EIO IPI发送失败或执行超时
 */
s32 smp_call_function_sync(cpu_set_t *cpus, void (*func)(void *), void *info);

#else

static inline void smp_master_init(void) {}
static inline void smp_slave_init(struct cpudev *cpu) {}
static inline void smp_slave_add_func(smp_slave_func_t func, void *data) {}
static inline s32 smp_call_function_async(cpu_set_t *cpus, void (*func)(void *), void *info)
{
    return 0;
}
static inline s32 smp_call_function_sync(cpu_set_t *cpus, void (*func)(void *), void *info)
{
    return 0;
}

#endif

#endif /* __KERNEL_SMP_H__ */
