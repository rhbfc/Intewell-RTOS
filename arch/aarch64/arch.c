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
 * @file arch.c
 * @brief AArch64 架构上下文辅助接口。
 */

#include <cpu.h>
#include <ttos.h>
#include <ttosProcess.h>
#include <ttos_arch.h>

#define GENMASK_ULL(h, l)                                                      \
    (((~0ULL) << (l)) & (~0ULL >> (sizeof(long long) * 8 - 1 - (h))))

#define GEN_DEBUG_WRITE_REG(reg_name)                                          \
    static void write_##reg_name(int num, uint64_t val)                        \
    {                                                                          \
        switch (num)                                                           \
        {                                                                      \
        case 0:                                                                \
            write_##reg_name##0_el1(val);                                      \
            break;                                                             \
        case 1:                                                                \
            write_##reg_name##1_el1(val);                                      \
            break;                                                             \
        case 2:                                                                \
            write_##reg_name##2_el1(val);                                      \
            break;                                                             \
        case 3:                                                                \
            write_##reg_name##3_el1(val);                                      \
            break;                                                             \
        case 4:                                                                \
            write_##reg_name##4_el1(val);                                      \
            break;                                                             \
        case 5:                                                                \
            write_##reg_name##5_el1(val);                                      \
            break;                                                             \
        case 6:                                                                \
            write_##reg_name##6_el1(val);                                      \
            break;                                                             \
        case 7:                                                                \
            write_##reg_name##7_el1(val);                                      \
            break;                                                             \
        case 8:                                                                \
            write_##reg_name##8_el1(val);                                      \
            break;                                                             \
        case 9:                                                                \
            write_##reg_name##9_el1(val);                                      \
            break;                                                             \
        case 10:                                                               \
            write_##reg_name##10_el1(val);                                     \
            break;                                                             \
        case 11:                                                               \
            write_##reg_name##11_el1(val);                                     \
            break;                                                             \
        case 12:                                                               \
            write_##reg_name##12_el1(val);                                     \
            break;                                                             \
        case 13:                                                               \
            write_##reg_name##13_el1(val);                                     \
            break;                                                             \
        case 14:                                                               \
            write_##reg_name##14_el1(val);                                     \
            break;                                                             \
        case 15:                                                               \
            write_##reg_name##15_el1(val);                                     \
            break;                                                             \
        default:                                                               \
            break;                                                             \
        }                                                                      \
    }

GEN_DEBUG_WRITE_REG(dbgbcr)
GEN_DEBUG_WRITE_REG(dbgbvr)
GEN_DEBUG_WRITE_REG(dbgwcr)
GEN_DEBUG_WRITE_REG(dbgwvr)

static inline int get_num_brps(void)
{
    u64 dfr0 = read_id_aa64dfr0_el1();
    return (dfr0 & GENMASK_ULL(15, 12)) >> 12;
}

static inline int get_num_wrps(void)
{
    u64 dfr0 = read_id_aa64dfr0_el1();
    return (dfr0 & GENMASK_ULL(23, 20)) >> 20;
}

void reset_debug_state(void)
{
    uint8_t brps;
    uint8_t wrps;
    uint8_t i;

    write_osdlr_el1(0);
    write_oslar_el1(0);
    isb();

    write_mdscr_el1(0);
    write_contextidr_el1(0);

    brps = get_num_brps();
    for (i = 0; i <= brps; i++)
    {
        write_dbgbcr(i, 0);
        write_dbgbvr(i, 0);
    }

    wrps = get_num_wrps();
    for (i = 0; i <= wrps; i++)
    {
        write_dbgwcr(i, 0);
        write_dbgwvr(i, 0);
    }

    isb();
}

void arch_switch_context_set_stack(T_TBSP_TaskContext *ctx, long sp)
{
    ctx->sp = sp;
}

void arch_context_set_return(arch_exception_context_t *context, long value)
{
    context->regs[0] = value;
}

long arch_context_get_return_val(arch_exception_context_t *context)
{
    return context->regs[0];
}

void arch_context_set_stack(arch_exception_context_t *context, long value)
{
    context->sp = value;
}

long arch_context_thread_init(arch_exception_context_t *context)
{
    return 0;
}

long arch_context_get_args(arch_exception_context_t *context, int index)
{
    if (index <= 31)
    {
        return context->regs[index];
    }
    return -1;
}
void restore_raw_context(arch_exception_context_t *context);
void restore_hw_debug(pcb_t pcb);

void restore_context(arch_exception_context_t *context)
{
    if (GET_EL(context->cpsr) == MODE_EL0)
    {
        /* 返回用户态前先处理挂起信号，并按需恢复硬件调试状态。 */
        arch_do_signal(context);

        pcb_t pcb = ttosProcessSelf();
        if (pcb_get(pcb))
        {
            TTOS_TaskEnterUserHook(pcb->taskControlId);
            pcb_put(pcb);
        }
    }
    restore_raw_context(context);
}
