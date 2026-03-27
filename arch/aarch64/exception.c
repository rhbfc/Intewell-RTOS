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
 * @file exception.c
 * @brief AArch64 IRQ/syscall 顶层入口实现。
 */

/************************头 文 件******************************/
#include "exception_internal.h"
#include <arch_helpers.h>
#include <cpu.h>
#include <errno.h>
#include <process_signal.h>
#include <syscalls.h>
#include <ttos.h>
#include <ttosInterTask.inl>
#include <ttos_pic.h>

#define KLOG_LEVEL KLOG_INFO
#undef KLOG_TAG
#define KLOG_TAG "Exception"
#include <klog.h>

/************************外部声明******************************/
void restore_context(void *context);
extern size_t intNestLevel[CONFIG_MAX_CPUS];
void ttosSchedule(void);
extern syscall_func syscall_table[CONFIG_SYSCALL_NUM];
extern syscall_func syscall_extent_table[CONFIG_EXTENT_SYSCALL_NUM];

/************************全局变量******************************/
size_t irqstack[CONFIG_MAX_CPUS][8] = {0};
size_t fiqstack[CONFIG_MAX_CPUS][8] = {0};
size_t svcstack[CONFIG_MAX_CPUS][8] = {0};
size_t undefstack[CONFIG_MAX_CPUS][8] = {0};
size_t abortstack[CONFIG_MAX_CPUS][8] = {0};

/************************函数实现******************************/

/**
 * @brief
 *    中断处理程序
 * @param[in] context 中断上下文
 * @retval 无
 */
void do_irq(arch_int_context_t *context)
{
    int ret;
    unsigned int from_cpu;
    unsigned int irq = 0;
    int cpuid;
    TASK_ID task = ttosGetRunningTask();

    aarch64_exception_save_user_entry_context(context, task);

    cpuid = cpuid_get();

    ret = ttos_pic_irq_ack(&from_cpu, &irq);
    if (ret == 0)
    {
        ttosDisableScheduleLevel[cpuid]++;
        intNestLevel[cpuid]++;

        ttos_pic_irq_handle(irq);
        ttos_pic_irq_eoi(irq, from_cpu);

        intNestLevel[cpuid]--;
        ttosDisableScheduleLevel[cpuid]--;
    }

    ttosSchedule();

    set_context_type(context, IRQ_CONTEXT);
    restore_context(context);

    while (1)
        ;
}

/**
 * @brief
 *    syscall处理程序
 * @param[in] context 系统调用上下文
 * @retval 无
 */
void do_syscall(arch_exception_context_t *context)
{
    long long ret = 0;
    int syscall_num;
    TASK_ID task = ttosGetRunningTask();

    syscall_num = context->regs[8];

    aarch64_exception_save_user_entry_context(context, task);
    set_context_type(context, SYSCALL_CONTEXT);

    arch_cpu_int_enable();

    if (is_extent_syscall_num(syscall_num))
    {
        syscall_num -= CONFIG_EXTENT_SYSCALL_NUM_START;
        if (syscall_num == 0)
        {
            ret = syscall_extent_table[syscall_num]((long)context, 0, 0, 0, 0, 0);
        }
        else
        {
            ret = syscall_extent_table[syscall_num](context->regs[0], context->regs[1],
                                                    context->regs[2], context->regs[3],
                                                    context->regs[4], context->regs[5]);
        }

        context->regs[0] = ret;
    }
    else if (syscall_num >= CONFIG_SYSCALL_NUM)
    {
        KLOG_I("syscall num %d great than:%d\n", syscall_num, CONFIG_SYSCALL_NUM - 1);
    }
    else
    {
        if (syscall_table[syscall_num])
        {
            ret = syscall_dispatch(syscall_num, context->regs[0], context->regs[1],
                                   context->regs[2], context->regs[3], context->regs[4],
                                   context->regs[5]);
            context->regs[0] = ret;
        }
        else
        {
            KLOG_E("syscall_table[%d] is NULL\n", syscall_num);
            context->regs[0] = -ENOSYS;
        }
    }

    restore_context(context);
}
