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
 * @file uaccess.S
 * @brief AArch64 用户态访问汇编辅助实现。
 */

#define ASM_USE
#include <asm.h>
#include <asm_fault_recovery.h>

.text

/* 从用户态拷贝到内核态，异常时通过 fault recovery 返回 -EFAULT。 */
ENTRY(arch_copy_from_user_asm)
    mov     x3, x0      /* dst */
    mov     x4, x1      /* src(user) */
    mov     x5, x2      /* len */
    mov     w0, #0
    cbz     x5, 90f

10:
    cmp     x5, #16
    blt     20f
11:
    ldtr    x6, [x4]
    FAULT_RECOVERY_EMIT_ERR(11b, 99f, w0)
12:
    ldtr    x7, [x4, #8]
    FAULT_RECOVERY_EMIT_ERR(12b, 99f, w0)
    stp     x6, x7, [x3]
    add     x4, x4, #16
    add     x3, x3, #16
    sub     x5, x5, #16
    b       10b

20:
    tbz     x5, #3, 30f
21:
    ldtr    x6, [x4]
    FAULT_RECOVERY_EMIT_ERR(21b, 99f, w0)
    str     x6, [x3]
    add     x4, x4, #8
    add     x3, x3, #8

30:
    tbz     x5, #2, 40f
31:
    ldtr    w6, [x4]
    FAULT_RECOVERY_EMIT_ERR(31b, 99f, w0)
    str     w6, [x3]
    add     x4, x4, #4
    add     x3, x3, #4

40:
    tbz     x5, #1, 50f
41:
    ldtrh   w6, [x4]
    FAULT_RECOVERY_EMIT_ERR(41b, 99f, w0)
    strh    w6, [x3]
    add     x4, x4, #2
    add     x3, x3, #2

50:
    tbz     x5, #0, 90f
51:
    ldtrb   w6, [x4]
    FAULT_RECOVERY_EMIT_ERR(51b, 99f, w0)
    strb    w6, [x3]

90:
    ret
99:
    ret
ENDPROC(arch_copy_from_user_asm)

/* 从内核态拷贝到用户态，异常时通过 fault recovery 返回 -EFAULT。 */
ENTRY(arch_copy_to_user_asm)
    mov     x3, x0      /* dst(user) */
    mov     x4, x1      /* src */
    mov     x5, x2      /* len */
    mov     w0, #0
    cbz     x5, 90f

10:
    cmp     x5, #16
    blt     20f
    ldp     x6, x7, [x4]
11:
    sttr    x6, [x3]
    FAULT_RECOVERY_EMIT_ERR(11b, 99f, w0)
12:
    sttr    x7, [x3, #8]
    FAULT_RECOVERY_EMIT_ERR(12b, 99f, w0)
    add     x4, x4, #16
    add     x3, x3, #16
    sub     x5, x5, #16
    b       10b

20:
    tbz     x5, #3, 30f
    ldr     x6, [x4]
21:
    sttr    x6, [x3]
    FAULT_RECOVERY_EMIT_ERR(21b, 99f, w0)
    add     x4, x4, #8
    add     x3, x3, #8

30:
    tbz     x5, #2, 40f
    ldr     w6, [x4]
31:
    sttr    w6, [x3]
    FAULT_RECOVERY_EMIT_ERR(31b, 99f, w0)
    add     x4, x4, #4
    add     x3, x3, #4

40:
    tbz     x5, #1, 50f
    ldrh    w6, [x4]
41:
    sttrh   w6, [x3]
    FAULT_RECOVERY_EMIT_ERR(41b, 99f, w0)
    add     x4, x4, #2
    add     x3, x3, #2

50:
    tbz     x5, #0, 90f
    ldrb    w6, [x4]
51:
    sttrb   w6, [x3]
    FAULT_RECOVERY_EMIT_ERR(51b, 99f, w0)

90:
    ret
99:
    ret
ENDPROC(arch_copy_to_user_asm)

ENTRY(arch_strcpy_to_user_asm)
    mov     x2, x0      /* dst(user) */
    mov     x3, x1      /* src */
    mov     w0, #0      /* len */

10:
    ldrb    w4, [x3], #1
11:
    sttrb   w4, [x2]
    FAULT_RECOVERY_EMIT_ERR(11b, 99f, w0)
    add     x2, x2, #1
    cbz     w4, 90f
    add     w0, w0, #1
    b       10b

90:
    ret
99:
    ret
ENDPROC(arch_strcpy_to_user_asm)

ENTRY(arch_clear_user_asm)
    mov     x2, x0      /* dst(user) */
    mov     x3, x1      /* remaining bytes */
    cbz     x3, 90f

10:
    cmp     x3, #8
    blt     30f
11:
    sttr    xzr, [x2]
    FAULT_RECOVERY_EMIT_RESUME(11b, 99f)
    add     x2, x2, #8
    sub     x3, x3, #8
    b       10b

30:
    tbz     x3, #2, 40f
31:
    sttr    wzr, [x2]
    FAULT_RECOVERY_EMIT_RESUME(31b, 99f)
    add     x2, x2, #4
    sub     x3, x3, #4

40:
    tbz     x3, #1, 50f
41:
    sttrh   wzr, [x2]
    FAULT_RECOVERY_EMIT_RESUME(41b, 99f)
    add     x2, x2, #2
    sub     x3, x3, #2

50:
    tbz     x3, #0, 90f
51:
    sttrb   wzr, [x2]
    FAULT_RECOVERY_EMIT_RESUME(51b, 99f)
    sub     x3, x3, #1

90:
    mov     x0, xzr
    ret
99:
    mov     x0, x3
    ret
ENDPROC(arch_clear_user_asm)
