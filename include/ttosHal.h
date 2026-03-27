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
 * @file： ttosHal.h
 * @brief：
 *      <li>提供定义ttos需要使用到的HACL相关的宏定义、类型定义和接口声明。</li>
 * @implements: DKB
 */

#ifndef _TTOSHAL_H
#define _TTOSHAL_H

/************************头文件********************************/
#ifndef ASM_USE
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif /* _GNU_SOURCE */
#include <commonTypes.h>
#include <sched.h>
#include <ttosUtils.h>
#endif /* ASM_USE */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/************************宏定义********************************/

/************************类型定义******************************/
#ifndef ASM_USE
typedef struct cpu_set_t cpu_set_t;
#endif /* ASM_USE */

/************************接口声明******************************/
#ifndef ASM_USE

/* 虚拟外部中断处理函数指针类型定义 */
typedef T_VOID (*T_VBSP_EXINT_HANDLER) (T_ULONG vector);
typedef T_VBSP_EXINT_HANDLER T_TTOS_ISR_HANDLER;

/*
 *如果对应的中断使用共享中断的方式提供，系统会依次执行安装的
 *所有共享中断处理程序。用户需要在共享中断处理程序中判断是否
 *是对应设备的中断，只有是对应设备的中断才进行相应的处理，否
 *则直接返回。
 */
typedef T_VOID (*T_SHARE_INT_HANDLER) (T_UBYTE intNum);
typedef T_SHARE_INT_HANDLER T_TTOS_SHARE_INT_HANDLER;

#if CONFIG_SMP == 1
T_VOID apStart (T_UWORD cpuID);
#endif
T_BOOL  ttosIsISR (T_VOID);
T_ULONG ttosGetIntNestLevel (T_VOID);
T_BOOL  ttosIsTickISR (T_VOID);
T_ULONG ttosGetTickIntNestLevel (T_VOID);
T_VOID  ttosIncTickIntNestLevel (T_VOID);
T_VOID  ttosDecTickIntNestLevel (T_VOID);

#if CONFIG_SMP == 1

extern cpu_set_t *ttosGetEnabledCpuSet (void);
extern cpu_set_t *ttosGetReservedCpuSet (void);
/* 已使能的CPU集合 */
#define TTOS_CPUSET_ENABLED() (ttosGetEnabledCpuSet ())

/* 已预留的CPU集合 */
#define TTOS_CPUSET_RESERVED() (ttosGetReservedCpuSet ())

/* 用户需要使能的cpu的个数 */
#define TTOS_CONFIG_CPUS() (ttosConfigCpuNum)

/* 用户配置的cpu个数 */
T_EXTERN T_UWORD ttosConfigCpuNum;
#endif /* CONFIG_SMP */
#endif /*ASM_USE*/

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _TTOSHAL_H */
