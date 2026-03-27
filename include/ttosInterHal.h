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
 * @file    ttosInterHal.h
 * @brief
 *  <li>提供TBSP相关的宏定义和接口声明。</li>
 * @implements： DT.6
 */

#ifndef _TTOSINTERHAL_H
#define _TTOSINTERHAL_H

/************************头文件********************************/
#include <spinlock.h>
#include <ttosTypes.h>

#ifdef __cplusplus
extern "C"
{
#endif

/************************宏定义********************************/
/* CPU状态字相关定义*/
#define TBSP_MSR_TYPE T_ULONG

/* TODO */
#define TBSP_GLOBALINT_DISABLE(_msr) ttos_int_lock (_msr)
#define TBSP_GLOBALINT_ENABLE(_msr)  ttos_int_unlock (_msr)
#define TBSP_GLOBALINT_OPEN()        arch_cpu_int_enable ()

#if CONFIG_SMP == 1
#if defined(TTOS_SPINLOCK_WITH_RECURSION)
T_EXTERN DECLARE_SPINLOCK (ttosKernelLockVar);
#define TTOS_KERNEL_LOCK()   (kernel_spin_lock (&ttosKernelLockVar))
#define TTOS_KERNEL_UNLOCK() (kernel_spin_unlock (&ttosKernelLockVar))
#else
T_EXTERN DECLARE_KERNEL_SPINLOCK (ttosKernelLockVar);
#define TTOS_KERNEL_LOCK()   (kernel_spin_lock_with_recursion (&ttosKernelLockVar))
#define TTOS_KERNEL_UNLOCK() (kernel_spin_unlock_with_recursion (&ttosKernelLockVar))
#endif
#else
#define TTOS_KERNEL_LOCK()
#define TTOS_KERNEL_UNLOCK()
#endif

#define TBSP_GLOBALINT_DISABLE_WITH_LOCK(_msr)                                 \
    ({                                                                         \
        TBSP_GLOBALINT_DISABLE (_msr);                                         \
        TTOS_KERNEL_LOCK ();                                                   \
    })

#define TBSP_GLOBALINT_ENABLE_WITH_LOCK(_msr)                                  \
    ({                                                                         \
        TTOS_KERNEL_UNLOCK ();                                                 \
        TBSP_GLOBALINT_ENABLE (_msr);                                          \
    })

/************************类型定义******************************/
/************************接口声明******************************/
T_VOID  tbspInit (T_VOID);
T_UWORD tbspSaveTaskContext (T_TBSP_TaskContext *context, T_UWORD cpuID);
T_VOID  tbspRestoreTaskContext (T_TBSP_TaskContext *context, T_UWORD cpuID);
T_VOID  tbspSetPWaitTick (T_UWORD ticks);
T_VOID  tbspSetPExpireTick (T_UWORD ticks);
T_UWORD tbspGetPWaitTick (T_VOID);
T_UWORD tbspGetPExpireTick (T_VOID);
T_VOID  tbspLoadStack (T_VOID *stackTop);
T_VOID  idleTaskEntryLoad (T_VOID *stackTop);

#ifdef _HARD_FLOAT_
T_VOID tbspInitFPSCR (T_VOID);
#endif

#ifdef __cplusplus
}
#endif

#endif
