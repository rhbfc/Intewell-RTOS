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

#include "syscall_internal.h"
#include <arch/arch.h>
#include <ttosProcess.h>
#include <process_signal.h>
#include <context.h>

DEFINE_SYSCALL (rt_sigreturn, (void))
{
    pcb_t pcb = ttosProcessSelf();

    rt_sigreturn(pcb->exception_context_raw_ptr);

    return arch_context_get_return_val(pcb->exception_context_raw_ptr);
}
