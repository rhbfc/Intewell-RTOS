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

#include <context.h>
#include <ttos.h>
#include <ttosInterHal.h>

void arch_context_save_cpu_state(T_TTOS_TaskControlBlock *task, long msr)
{
    task->switchContext.cpsr = msr;
}

void arch_context_restore_cpu_state(T_TTOS_TaskControlBlock *task)
{
    TBSP_GLOBALINT_ENABLE(task->switchContext.cpsr);
}