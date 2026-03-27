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
 * @file signal.S
 * @brief AArch64 信号返回用户态的代码片段。
 */

#define ASM_USE
#include <asm.h>
#include <cpu.h>
#include <asm-offsets.h>
#include <syscall.h>

.align 4
.global sigreturn_code
sigreturn_code:
    /* 用户态信号 trampoline 调用 rt_sigreturn 系统调用。 */
    mov x8, #__NR_rt_sigreturn
    svc #0
