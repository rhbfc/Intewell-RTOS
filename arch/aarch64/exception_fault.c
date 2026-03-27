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
 * @file exception_fault.c
 * @brief AArch64 同步异常与故障日志处理实现。
 */

#include "ttosMM.h"
#include "exception_internal.h"
#include <arch_helpers.h>
#include <cpu.h>
#include <fault_recovery_table.h>
#include <inttypes.h>
#include <process_signal.h>
#include <signal.h>
#include <symtab.h>
#include <ttosInterTask.inl>
#include <uaccess.h>

#define KLOG_LEVEL KLOG_INFO
#undef KLOG_TAG
#define KLOG_TAG "Exception"
#include <klog.h>

#define ESR_ELx_EC_SHIFT 26
#define ESR_ELx_EC_MASK  (0x3FUL << ESR_ELx_EC_SHIFT)
#define ESR_ELx_EC(esr)  (((esr) & ESR_ELx_EC_MASK) >> ESR_ELx_EC_SHIFT)
#define ESR_ELx_IL_SHIFT 25
#define ESR_ELx_IL       (1UL << ESR_ELx_IL_SHIFT)
#define ESR_ELx_FSC      0x3fUL

struct fault_info
{
    int sig;
    int code;
    const char *name;
};

void restore_context(void *context);
void backtrace_r(const char *cookie, uintptr_t frame_address);
const char *esr_get_class_string(unsigned long esr);
extern unsigned char tmp_stack[PAGE_SIZE * CONFIG_MAX_CPUS] __attribute__((aligned(PAGE_SIZE)));

static const struct fault_info fault_info[] = {
    {SIGKILL, SI_KERNEL, "ttbr address size fault"},
    {SIGKILL, SI_KERNEL, "level 1 address size fault"},
    {SIGKILL, SI_KERNEL, "level 2 address size fault"},
    {SIGKILL, SI_KERNEL, "level 3 address size fault"},
    {SIGSEGV, SEGV_MAPERR, "level 0 translation fault"},
    {SIGSEGV, SEGV_MAPERR, "level 1 translation fault"},
    {SIGSEGV, SEGV_MAPERR, "level 2 translation fault"},
    {SIGSEGV, SEGV_MAPERR, "level 3 translation fault"},
    {SIGSEGV, SEGV_ACCERR, "level 0 access flag fault"},
    {SIGSEGV, SEGV_ACCERR, "level 1 access flag fault"},
    {SIGSEGV, SEGV_ACCERR, "level 2 access flag fault"},
    {SIGSEGV, SEGV_ACCERR, "level 3 access flag fault"},
    {SIGSEGV, SEGV_ACCERR, "level 0 permission fault"},
    {SIGSEGV, SEGV_ACCERR, "level 1 permission fault"},
    {SIGSEGV, SEGV_ACCERR, "level 2 permission fault"},
    {SIGSEGV, SEGV_ACCERR, "level 3 permission fault"},
    {SIGBUS, BUS_OBJERR, "synchronous external abort"},
    {SIGSEGV, SEGV_MTESERR, "synchronous tag check fault"},
    {SIGKILL, SI_KERNEL, "unknown 18"},
    {SIGKILL, SI_KERNEL, "level -1 (translation table walk)"},
    {SIGKILL, SI_KERNEL, "level 0 (translation table walk)"},
    {SIGKILL, SI_KERNEL, "level 1 (translation table walk)"},
    {SIGKILL, SI_KERNEL, "level 2 (translation table walk)"},
    {SIGKILL, SI_KERNEL, "level 3 (translation table walk)"},
    {SIGBUS, BUS_OBJERR, "synchronous parity or ECC error"},
    {SIGKILL, SI_KERNEL, "unknown 25"},
    {SIGKILL, SI_KERNEL, "unknown 26"},
    {SIGKILL, SI_KERNEL, "level -1 synchronous parity error (translation table walk)"},
    {SIGKILL, SI_KERNEL, "level 0 synchronous parity error (translation table walk)"},
    {SIGKILL, SI_KERNEL, "level 1 synchronous parity error (translation table walk)"},
    {SIGKILL, SI_KERNEL, "level 2 synchronous parity error (translation table walk)"},
    {SIGKILL, SI_KERNEL, "level 3 synchronous parity error (translation table walk)"},
    {SIGKILL, SI_KERNEL, "unknown 32"},
    {SIGBUS, BUS_ADRALN, "alignment fault"},
    {SIGKILL, SI_KERNEL, "unknown 34"},
    {SIGKILL, SI_KERNEL, "unknown 35"},
    {SIGKILL, SI_KERNEL, "unknown 36"},
    {SIGKILL, SI_KERNEL, "unknown 37"},
    {SIGKILL, SI_KERNEL, "unknown 38"},
    {SIGKILL, SI_KERNEL, "unknown 39"},
    {SIGKILL, SI_KERNEL, "unknown 40"},
    {SIGKILL, SI_KERNEL, "level -1 address size fault"},
    {SIGKILL, SI_KERNEL, "unknown 42"},
    {SIGSEGV, SEGV_MAPERR, "level -1 translation fault"},
    {SIGKILL, SI_KERNEL, "unknown 44"},
    {SIGKILL, SI_KERNEL, "unknown 45"},
    {SIGKILL, SI_KERNEL, "unknown 46"},
    {SIGKILL, SI_KERNEL, "unknown 47"},
    {SIGKILL, SI_KERNEL, "TLB conflict abort"},
    {SIGKILL, SI_KERNEL, "Unsupported atomic hardware update fault"},
    {SIGKILL, SI_KERNEL, "unknown 50"},
    {SIGKILL, SI_KERNEL, "unknown 51"},
    {SIGKILL, SI_KERNEL, "implementation fault (lockdown abort)"},
    {SIGBUS, BUS_OBJERR, "implementation fault (unsupported exclusive)"},
    {SIGKILL, SI_KERNEL, "unknown 54"},
    {SIGKILL, SI_KERNEL, "unknown 55"},
    {SIGKILL, SI_KERNEL, "unknown 56"},
    {SIGKILL, SI_KERNEL, "unknown 57"},
    {SIGKILL, SI_KERNEL, "unknown 58"},
    {SIGKILL, SI_KERNEL, "unknown 59"},
    {SIGKILL, SI_KERNEL, "unknown 60"},
    {SIGKILL, SI_KERNEL, "section domain fault"},
    {SIGKILL, SI_KERNEL, "page domain fault"},
    {SIGKILL, SI_KERNEL, "unknown 63"},
};

static inline const struct fault_info *esr_to_fault_info(unsigned long esr)
{
    return fault_info + (esr & ESR_ELx_FSC);
}

static inline bool is_user_illegal_exception(struct arch_context *context)
{
    return (ESR_ELx_IL & context->esr) && ESR_ELx_EC(context->esr) == 0;
}

static void handle_user_illegal_exception(struct arch_context *context, pcb_t pcb)
{
    pid_t pid;

    if (!pcb || !is_user_illegal_exception(context))
        return;

    pid = get_process_pid(pcb);
    kernel_signal_send(pid, TO_PROCESS, SIGILL, SI_KERNEL, NULL);
    set_context_type(context, EXCEPTION_CONTEXT);
    restore_context(context);
}

static void log_common_exception(struct arch_context *context, const struct fault_info *info,
                                 const char *title)
{
    u64 far = read_far_el1();

    KLOG_EMERG("%s", title);
    KLOG_EMERG("Case by: %s ESR: %p EC:0x%x Fault Address: %p(0x%" PRIx64 ")",
               esr_get_class_string(context->esr), (void *)context->esr,
               ESR_ELx_EC(context->esr), (void *)far, mm_kernel_v2p((virt_addr_t)far));
    KLOG_EMERG("Reason: %s", info->name);
    KLOG_EMERG("RegMap:");
    KLOG_EMERG("========================================");
    KLOG_EMERG("cpacr     :%p", (void *)context->cpacr);
    KLOG_EMERG("vector    :%p", (void *)context->vector);
    KLOG_EMERG("cpsr      :%p", (void *)context->cpsr);
    KLOG_EMERG("esr       :%p", (void *)context->esr);
    KLOG_EMERG("sp        :%p", (void *)context->sp);
}

static void log_user_registers(struct arch_context *context)
{
    int i;

    KLOG_EMERG("elr       :%p", (void *)context->elr);

    for (i = 0; i < 31; i++)
    {
        switch (i)
        {
        case 30:
            KLOG_EMERG("x%d(lr)   %s:%p", i, i / 10 ? "" : " ", (void *)context->regs[i]);
            break;
        case 29:
            KLOG_EMERG("x%d(fp)   %s:%p", i, i / 10 ? "" : " ", (void *)context->regs[i]);
            break;
        default:
            KLOG_EMERG("x%d       %s:%p", i, i / 10 ? "" : " ", (void *)context->regs[i]);
            break;
        }
    }

    KLOG_EMERG("========================================");
}

static void log_kernel_registers(struct arch_context *context)
{
    int i;
    size_t symsize;
    const struct symtab_item *sym = allsyms_findbyvalue((void *)context->elr, &symsize);

    if (sym)
    {
        KLOG_EMERG("elr       :%p (%s + %p)", (void *)context->elr, sym->sym_name,
                   (void *)(context->elr - (u64)sym->sym_value));
    }
    else
    {
        KLOG_EMERG("elr       :%p", (void *)context->elr);
    }

    for (i = 0; i < 31; i++)
    {
        switch (i)
        {
        case 30:
        {
            size_t lr_symsize;
            const struct symtab_item *lr_sym =
                allsyms_findbyvalue((void *)context->regs[i], &lr_symsize);

            KLOG_EMERG("x%d(lr)   %s:%p (%s + %p)", i, i / 10 ? "" : " ",
                       (void *)context->regs[i], lr_sym->sym_name,
                       (void *)(context->regs[i] - (u64)lr_sym->sym_value));
            break;
        }
        case 29:
            KLOG_EMERG("x%d(fp)   %s:%p", i, i / 10 ? "" : " ", (void *)context->regs[i]);
            break;
        default:
            KLOG_EMERG("x%d       %s:%p", i, i / 10 ? "" : " ", (void *)context->regs[i]);
            break;
        }
    }

    KLOG_EMERG("========================================");
}

static void log_exception_task(struct arch_context *context, TASK_ID task, bool kernel_mode)
{
    if (!kernel_access_check(task, sizeof(*task), UACCESS_R))
        return;

    KLOG_EMERG("Exception Task: %s(%d) on CPU[%d]", task->objCore.objName,
               kernel_access_check(task->ppcb, sizeof(struct T_TTOS_ProcessControlBlock *),
                                   UACCESS_R) &&
                       !(task->ppcb)
                   ? get_process_pid((pcb_t)task->ppcb)
                   : -1,
               cpuid_get());

    if (!kernel_mode)
        return;

    if ((unsigned long)context >= (unsigned long)tmp_stack &&
        (unsigned long)context <= ((unsigned long)tmp_stack + sizeof(tmp_stack)))
    {
        KLOG_EMERG("Stack OverFlow");
        KLOG_EMERG("OverFlow sp:%p", (void *)context->err_sp);
        context->sp = context->err_sp;
        KLOG_EMERG("Kernel Stack: %p-%p", task->stackStart, task->kernelStackTop);
    }
}

static void log_kernel_exception(struct arch_context *context, TASK_ID task,
                                 const struct fault_info *info)
{
    log_common_exception(context, info, "================Kernel Exception================");
    log_kernel_registers(context);
    log_exception_task(context, task, true);
    backtrace_r("exception", context->regs[29]);
}

static void log_user_exception(struct arch_context *context, TASK_ID task,
                               const struct fault_info *info)
{
    log_common_exception(context, info, "================User Exception================");
    log_user_registers(context);
    log_exception_task(context, task, false);
}

static void handle_user_fault_signal(struct arch_context *context, pcb_t pcb,
                                     const struct fault_info *info)
{
    pid_t pid;

    if (!pcb || !info)
        return;

    pid = get_process_pid(pcb);
    kernel_signal_send(pid, TO_PROCESS, info->sig, info->code, NULL);
    set_context_type(context, EXCEPTION_CONTEXT);

    if (aarch64_exception_user_context_valid(context))
    {
        restore_context(context);
    }
    else
    {
        arch_do_signal(context);
    }
}

void do_exception(struct arch_context *context)
{
    TASK_ID task = ttosGetRunningTask();
    const struct fault_info *info = esr_to_fault_info(context->esr);
    pcb_t pcb = aarch64_exception_save_user_entry_context(context, task);

#ifdef CONFIG_SUPPORT_FAULT_RECOVERY
    if (aarch64_exception_from_kernel(context) && try_apply_fault_recovery(context))
    {
        restore_context(context);
    }
#endif

    handle_user_illegal_exception(context, pcb);

    if (aarch64_exception_from_kernel(context))
    {
        log_kernel_exception(context, task, info);
    }
    else
    {
        log_user_exception(context, task, info);
    }

    if (task)
    {
        if (aarch64_exception_from_user(context))
        {
            handle_user_fault_signal(context, pcb, info);
        }

        TTOS_SuspendTask(task);
    }

    while (1)
        ;
}

void do_data_abort(arch_exception_context_t *context)
{
    do_exception(context);
}
