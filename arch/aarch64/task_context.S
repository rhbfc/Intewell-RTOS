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
 * @file task_context.S
 * @brief AArch64 任务上下文与栈切换汇编辅助接口。
 */

/************************头 文 件******************************/
#define ASM_USE
#include <cpu.h>
#include <asm.h>
#include <ttosHal.h>
#include <arch_helpers.h>
/*
 * @brief
 *    进入 IDLE 任务函数
 * @param[in] stackTop: IDLE 任务栈顶
 * @retval 无
 */
/*  T_VOID idleTaskEntryLoad(T_VOID *stackTop)*/
ENTRY(idleTaskEntryLoad)

    /* 设置栈 */
    sub x0, x0, #32  /*预留安全空间*/
    bic x0, x0, #0xf  /*保证栈地址是16字节对齐的*/
    mov sp, x0

    /* 调用入口函数 */
    bl ttosIdleTaskEntry

1:
    b 1b
ENDPROC(idleTaskEntryLoad)

/*
 * @brief
 *    获取优先级位图中第一个置位的位序号
 * @param[in] priBitMap: 优先级位图
 * @retval 位序号
 */
/* T_UWORD tbspGetMSB(T_UWORD priBitMap) */
.text
.align  4
.global tbspGetMSB
tbspGetMSB:
    clz w0, w0
    ret

/*
 * @brief
 *    加载任务栈指针
 * @param[in] stackTop: 任务栈顶
 * @retval 无
 * @tracedREQ: RT.6
 * @implements: DT.6.15.PPC
 */
/*T_VOID tbspLoadStack(T_VOID *stackTop)*/
.text
.align  4
.global tbspLoadStack
tbspLoadStack:
    sub x0, x0, #32  /*预留安全空间*/
    bic x0, x0, #0xf /*保证栈地址是16字节对齐的*/
    mov sp, x0
    ret

/*
 * @brief
 *    保存任务运行上下文
 * @param[out] context: 任务上下文
 * @retval 0 任务被切换走后返回
 * @retval 1 任务被切换回后返回
 */

/*T_UWORD tbspSaveTaskContext(T_TBSP_TaskContext *context)*/
.text
.align  4
.global tbspSaveTaskContext
tbspSaveTaskContext:

#ifdef _HARD_FLOAT_
    stp d8, d9, [x0], #16
    stp d10, d11, [x0], #16
    stp d12, d13, [x0], #16
    stp d14, d15, [x0], #16

    mrs x1, fpsr
    mrs x2, fpcr

    /* 保存fpsr、fpcr */
    stp x1, x2, [x0], #16
#endif /* _HARD_FLOAT_ */

    stp x19, x20, [x0], #16
    stp x21, x22, [x0], #16
    stp x23, x24, [x0], #16
    stp x25, x26, [x0], #16
    stp x27, x28, [x0], #16
    stp x29, lr, [x0], #16

    mov x1, sp

    /* 改为在ttosTaskSchedule入口处保存,此处无需保存 */
    #mrs x2,daif

    /*ttosMSRAdress在任务上下文中占用的空间是64位的，高32位不会使用。*/
    #stp x1, x2, [x0], #16
    str x1, [x0], #16

    mrs x1, tpidr_el0
    mrs x2, ttbr0_el1

    stp x1, x2, [x0], #16

    mov x0, #0

    ret

/*
 * @brief
 *    恢复任务运行上下文
 * @param[in] context: 任务上下文
 * @retval 无
 */
/*T_VOID tbspRestoreTaskContext(T_TBSP_TaskContext *context)*/

.text
.align  4
.global tbspRestoreTaskContext
tbspRestoreTaskContext:

#ifdef _HARD_FLOAT_
    ldp d8, d9, [x0], #16
    ldp d10, d11, [x0], #16
    ldp d12, d13, [x0], #16
    ldp d14, d15, [x0], #16

    /*加载fpsr、fpcr*/
    ldp x1, x2, [x0], #16

    msr fpsr, x1
    restore_fpcr x2, x1
#endif /* _HARD_FLOAT_ */

    ldp x19, x20, [x0], #16
    ldp x21, x22, [x0], #16
    ldp x23, x24, [x0], #16
    ldp x25, x26, [x0], #16
    ldp x27, x28, [x0], #16
    ldp x29, lr, [x0], #16

    ldp x1, x2, [x0], #16
    mov sp, x1

    /* 改为在ttosTaskSchedule出口处恢复,此处无需恢复 */
    #msr daif,x2

    ldp x1, x2, [x0], #16

    msr tpidr_el0, x1
    msr ttbr0_el1, x2
    mov x0, #1
    ret
#ifdef _HARD_FLOAT_

.text
.align  4
.global fpscrInit
.global fpscrAsmInit
fpscrInit:
fpscrAsmInit:

    mov x2, #0
    msr fpsr, x2

    mov x0, #0
    restore_fpcr x0, x1
    ret

#endif /* _HARD_FLOAT_ */
