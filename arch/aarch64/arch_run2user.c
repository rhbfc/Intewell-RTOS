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
 * @file arch_run2user.c
 * @brief AArch64 用户态切换辅助实现。
 */

#include <assert.h>
#include <cpu.h>
#include <system/compiler.h>
#include <stdint.h>
#include <ttos.h>
#include <ttosProcess.h>

/* 重置调试状态。 */
void reset_debug_state(void);

/* 汇编入口，负责真正切换到用户态。 */
extern void return2user (uintptr_t args, uintptr_t sp, uintptr_t entry,
                         uintptr_t msr);

/**
 * @brief 将 CPU 从内核态切换到用户态
 * @note 该函数不会返回。
 */
__noreturn void arch_run2user(void)
{
    pcb_t pcb = ttosProcessSelf ();

    assert (pcb != NULL);

    /* 进入用户态前清理调试现场，并触发用户态进入钩子。 */
    reset_debug_state();
    TTOS_TaskEnterUserHook(TTOS_GetRunningTask ());

    /* 切换到用户态并从用户入口开始执行。 */
    return2user ((uintptr_t)pcb->args, (uintptr_t)pcb->userStack,
                 (uintptr_t)pcb->entry, SPSR_64(MODE_EL0, MODE_SP_EL0, 0));

    while (1)
        ;
}
