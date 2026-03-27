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
 * @file exception_context.c
 * @brief AArch64 异常上下文公共辅助实现。
 */

#include "exception_internal.h"
#include <cpu.h>
#include <process_signal.h>
#include <string.h>
#include <ttos.h>
#include <ttosInterTask.inl>
#include <uaccess.h>

void *exception_sp_get(struct arch_context *context)
{
    return (void *)context->sp;
}

bool aarch64_exception_from_user(const struct arch_context *context)
{
    return GET_EL(context->cpsr) == MODE_EL0;
}

bool aarch64_exception_from_kernel(const struct arch_context *context)
{
    return GET_EL(context->cpsr) == MODE_EL1;
}

static pcb_t task_get_user_pcb(TASK_ID task)
{
    if (!task || !task->ppcb)
        return NULL;

    return (pcb_t)task->ppcb;
}

bool aarch64_exception_user_context_valid(struct arch_context *context)
{
    if (!user_access_check((void *)context->sp, sizeof(context->sp), UACCESS_RW))
        return false;

    if (!user_access_check((void *)context->elr, sizeof(context->elr), UACCESS_R))
        return false;

    return true;
}

void set_context_type(struct arch_context *context, int type)
{
    context->type = type;
    context->ori_x0 = context->regs[0];
}

pcb_t aarch64_exception_save_user_entry_context(struct arch_context *context, TASK_ID task)
{
    pcb_t pcb;

    if (!aarch64_exception_from_user(context))
        return NULL;

    pcb = task_get_user_pcb(task);
    if (!pcb)
        return NULL;

    memcpy(&pcb->exception_context, context, sizeof(pcb->exception_context));

    pcb->exception_context_raw_ptr = context;

    TTOS_TaskEnterKernelHook(task);
    return pcb;
}
