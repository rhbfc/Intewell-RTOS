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

/**
 * @file： ttos_arch.h
 * @brief：
 *	    <li>TTOS依赖的体系结构相关头文件。</li>
 */
#ifndef _TTOS_ARCH_H
#define _TTOS_ARCH_H

/************************头文件********************************/
#include <arch/arch.h>
#include <trace.h>
#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/************************宏定义********************************/

#if defined(CONFIG_TRACE) && defined(CONFIG_TRACE_COMPETE)
T_UDWORD TTOS_GetCurrentSystemCount(T_VOID);
u32  cpuid_get(void);

extern uint64_t __ttos_int_disable_start[];   // 第一次关中断的时间戳
extern uint32_t __ttos_int_lock_depth[];      // 嵌套计数

__always_inline void int_check_trace(uint64_t duration)
{
    current_label: void *pc = &&current_label;
    trace_compete(pc, COMPETE_T_INT, duration);
}

#define ttos_int_lock(msr)                                              \
    do {                                                                \
        if (__ttos_int_lock_depth[cpuid_get()] == 0) {                  \
            __ttos_int_disable_start[cpuid_get()] =                     \
                TTOS_GetCurrentSystemCount();                           \
        }                                                               \
        __ttos_int_lock_depth[cpuid_get()]++;                           \
        arch_cpu_int_save(msr);                                         \
    } while (0)

#define ttos_int_unlock(msr)                                            \
    do {                                                                \
        if (__ttos_int_lock_depth[cpuid_get()] > 0) {                   \
            __ttos_int_lock_depth[cpuid_get()]--;                       \
            if (__ttos_int_lock_depth[cpuid_get()] == 0) {              \
                uint64_t __end = TTOS_GetCurrentSystemCount();          \
                uint64_t __interval = __end -                           \
                __ttos_int_disable_start[cpuid_get()];                  \
                if (__interval > INT_COMPETE_TICK)                      \
                    int_check_trace(__interval);                        \
            }                                                           \
        }                                                               \
        arch_cpu_int_restore(msr);                                      \
    } while (0)

#else
/* 定义内核使用的中断开关接口 */
#define ttos_int_lock(msr)   arch_cpu_int_save (msr)
#define ttos_int_unlock(msr) arch_cpu_int_restore (msr)

#endif

/************************类型定义******************************/

/************************接口声明******************************/

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _TTOS_ARCH_H */
