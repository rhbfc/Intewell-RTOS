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
 * @file： arch.h
 * @brief：
 *      <li>提供体统结构相关的宏定义、类型定义和接口声明。</li>
 */

#ifndef _ARCH_H
#define _ARCH_H

/************************头文件********************************/
#include <commonTypes.h>

#if defined(__arm__) || defined(__aarch64__)

#include <arch/arm_common/mmu.h>

#endif

#include <arch_types.h>

#ifdef __arm__
#include <arch/arm/context.h>
#include <arch/arm/int.h>
#include <arch/arm/elf.h>
#elif defined(__aarch64__)
#include <arch/aarch64/context.h>
#include <arch/aarch64/int.h>
#include <arch/aarch64/elf.h>
#include <arch/aarch64/barrier.h>
#elif defined(__x86_64__)
#include <arch/x86_64/asm.h>
#include <arch/x86_64/context.h>
#include <arch/x86_64/mmu.h>
#include <arch/x86_64/int.h>
#include <arch/x86_64/barrier.h>
#include <arch/x86_64/elf.h>
// #include <arch/x86_64/itlcommon.h>
#endif



#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/************************宏定义********************************/

/************************类型定义******************************/

/************************接口声明******************************/
T_UWORD zero_num_of_word_get (T_UWORD numVar);
T_UWORD word_lsb_get (T_UWORD numVar);

long arch_context_get_args (arch_exception_context_t *context, int index);
void arch_context_set_return (arch_exception_context_t *context, long value);
long arch_context_get_return_val(arch_exception_context_t *context);
void arch_switch_context_set_stack(T_TBSP_TaskContext *ctx, long sp);
void arch_context_set_stack (arch_exception_context_t *context, long value);
long arch_context_thread_init (arch_exception_context_t *context);

static inline uint64_t get_context_sp(struct arch_context* context)
{
    uint64_t sp;
#if defined (__arm__) || defined (__aarch64__)
    sp = context->sp;
#elif defined(__x86_64__)
    sp = context->rsp;
#else
    #error "Unsupported architecture"
#endif

    return sp;
}

static inline void* get_sp()
{
    void *sp;
#if defined(__x86_64__)
    __asm__ volatile ("movq %%rsp, %0" : "=r"(sp));
#elif defined (__arm__) || defined (__aarch64__)
    __asm__ volatile ("mov %0, sp" : "=r"(sp));
#else
    #error "Unsupported architecture"
#endif

    return sp;
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _ARCH_H */
