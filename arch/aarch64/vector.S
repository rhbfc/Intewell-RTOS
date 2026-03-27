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
 * @file： vector.S
 * @brief：
 *     <li>AArch64 异常向量表与异常现场保存逻辑。</li>
 */

/************************头 文 件******************************/
#define ASM_USE
#include <cpu.h>
#include <context.h>
#include <asm-offsets.h>
#include <asm.h>
#include <system/const.h>

/************************宏 定 义******************************/
#ifndef PAGE_SIZE
#define PAGE_SIZE 0x1000
#endif

.macro  ventry label
    .align 7
    b   \label
.endm

/* EL0 异常需要切换到用户进程对应的系统栈。 */
.macro  switch_to_vm_sysStack

    /* 在进入用户态时已重新设置SP_ELx指向当前CPU的系统栈，可直接切换到SP_ELx使用 */
    msr spsel, #MODE_SP_ELX
    sub sp, sp, (FP_CONTEXT_SIZE)
    stp    x29, x30, [sp, #-16]!

	mrs x29, mdscr_el1
	and x29, x29, #(~(1ULL))
	msr mdscr_el1, x29

    /* lock os lock */
    mov x29, #1
    msr oslar_el1, x29

.endm

/************************类型定义******************************/

/************************外部声明******************************/

/************************前向声明******************************/

/************************模块变量******************************/

/************************函数实现******************************/
/* 中断向量表基地址按 2^11 字节对齐。 */
.section ".vectors", "ax"
.align	 11
.globl vector_table
vector_table:
    /* Exception from CurrentEL (EL1t) with SP_EL0 (SPSEL = 0) */
    ventry  vector_unhandler    /* Synchronous */
    ventry  vector_unhandler      /* IRQ/vIRQ */
    ventry  vector_unhandler      /* FIQ/vFIQ */
    ventry  vector_unhandler    /* SError/vSError */

    /* Exception from CurrentEL (EL1h) with SP_ELn */
    ventry  vector_sync_sp_el1    /* Synchronous */
    ventry  vector_irq_sp_el1      /* IRQ/vIRQ */
    ventry  vector_unhandler      /* FIQ/vFIQ */
    ventry  vector_unhandler    /* SError/vSError */

    /* Exception from lower EL, aarch64 */
    ventry  vector_sync_aarch64    /* Synchronous */
    ventry  vector_irq_aarch64    /* IRQ/vIRQ */
    ventry  vector_unhandler    /* FIQ/vFIQ */
    ventry  vector_unhandler    /* SError/vSError */

    /* Exception from lower EL, aarch32 */
    ventry  vector_unhandler    /* Synchronous */
    ventry  vector_unhandler    /* IRQ/vIRQ */
    ventry  vector_unhandler    /* FIQ/vFIQ */
    ventry  vector_unhandler    /* SError/vSError */



save_float_context:
	/*
	 * 浮点上下文在内存中的布局为：
	 * 高地址
	 * T_ULONG fpcr;
	 * T_ULONG fpsr;
	 * __uint128_t vRegs[32];
	 * 低地址
	 */
    add x29, sp, #S_FP_CONTEXT_START
    stp q0,  q1,  [x29], #32
    stp q2,  q3,  [x29], #32
    stp q4,  q5,  [x29], #32
    stp q6,  q7,  [x29], #32
    stp q8,  q9,  [x29], #32
    stp q10,  q11,  [x29], #32
    stp q12,  q13,  [x29], #32
    stp q14,  q15,  [x29], #32
    stp q16,  q17,  [x29], #32
    stp q18,  q19,  [x29], #32
    stp q20,  q21,  [x29], #32
    stp q22,  q23,  [x29], #32
    stp q24,  q25,  [x29], #32
    stp q26,  q27,  [x29], #32
    stp q28,  q29,  [x29], #32
    stp q30,  q31,  [x29], #32

	MRS x1, FPCR
	MRS x0, FPSR
	stp x0, x1, [x29]
    ret

/*
 * Save (most of) the GP registers to the stack frame.
 * This is the first part of the shared routine called into from all entries.
 */
_exception_entry:
    stp    x27, x28, [sp, #-16]!
    stp    x25, x26, [sp, #-16]!
    stp    x23, x24, [sp, #-16]!
    stp    x21, x22, [sp, #-16]!
    stp    x19, x20, [sp, #-16]!
    stp    x17, x18, [sp, #-16]!
    stp    x15, x16, [sp, #-16]!
    stp    x13, x14, [sp, #-16]!
    stp    x11, x12, [sp, #-16]!
    stp    x9, x10, [sp, #-16]!
    stp    x7, x8, [sp, #-16]!
    stp    x5, x6, [sp, #-16]!
    stp    x3, x4, [sp, #-16]!
    stp    x1, x2, [sp, #-16]!
    b    _save_el_regs            /* jump to the second part */


    /*
 * Save exception specific context: ELR, SP, ELR, for all exception levels.
 * This is the second part of the shared routine called into from all entries.
 */
_save_el_regs:
    /* Could be running at EL3/EL2/EL1 */
    switch_el x11, 3f, 2f, 1f

3:
    mrs    x1, esr_el3
    mrs    x2, elr_el3
    mrs    x3, spsr_el3
    b    0f

2:
    mrs    x1, esr_el2
    mrs    x2, elr_el2
    mrs    x3, spsr_el2
    b    0f

1:
    mrs    x1, esr_el1
    mrs    x2, elr_el1
    mrs    x3, spsr_el1

0:
    /*保存elr、x0*/
    stp    x2, x0, [sp, #-16]!

    lsr x4, x3, #SPSR_EL_SHIFT
    and x4, x4, #SPSR_EL_MASK
    cmp x4, #SPSR_EL_EL0

    b.ne .non_EL0_exception

    /*获取用户态栈指针*/
    mrs x2, sp_el0

    b .save_esr_sp

.non_EL0_exception:
    /*计算异常产生时的栈指针，注意增加的立即数与上面保存的内容(T_RawExceptionContext)是相关联的。*/
    mov    x0, sp
	add    x2, x0, #(FP_CONTEXT_SIZE+31*8+8)   /* 在内核态产生的异常时，由于保存上下文，sp已经减去了(fpContext+regs[31]+elr)大小,在此加回，以获取产生异常时的sp */

.save_esr_sp:
    /*保存esr、sp*/
    stp    x1, x2, [sp, #-16]!

    /*获取EC作为向量号*/
    mrs x4, esr_el1
    lsr x4, x4, #ESR_EC_SHIFT
    and x4, x4, #ESR_EC_MASK

    /*保存vector、spsr*/
    stp    x4, x3, [sp, #-16]!

	/* x3 用来存 far */
	mrs x3, far_el1
    mrs x4, cpacr_el1

    /* x3的值此时无任何意义,保存type、cpacr,后续c代码会将上下文中的type成员设置为正确的值 */
    stp    x3, x4, [sp, #-16]!

#if ISR_DISABLE_FP==1
    /*
    * 此处显示的禁止浮点操作，需要进行浮点操作时，需要保存浮点上下文，在保存浮点上下文接口
    * 中会使能浮点操作。
    */
    mov x0, #CPACR_EL1_FPEN_DIS
    msr cpacr_el1, x0           /* Disable FP/SIMD */
#endif

    /* 跳过ori_x0 err_sp */
    sub sp, sp, #16

	/* 保存fpu context */
    mov x5, lr
	bl save_float_context
    mov lr, x5

    mov    x0, sp  /*pass execption context*/
    ret

    /*
 * Restore the exception return address, for all exception levels.
 * This is the first part of the shared routine called into from all entries.
 */
ENTRY(restore_raw_context)
    restore_raw_excContext x0
ENDPROC(restore_raw_context)


ENTRY(vector_unhandler)
    /*异常产生时，SPSel为1，即SP为SP_EL1，此处需要显示的将SP切换为SP_ELx。*/
    msr spsel, #MODE_SP_ELX

	sub sp, sp, (FP_CONTEXT_SIZE)

    /*需要在跳转之前保存x30(lr)*/
    stp    x29, x30, [sp, #-16]!

    bl    _exception_entry
    bl    do_exception
    mov   x0, sp
    b     restore_context
ENDPROC(vector_unhandler)

ENTRY(vector_sync_sp_el1)
    /*异常产生时，SPSel为1，即SP为SP_EL1，此处需要显示的将SP切换为SP_ELx。*/
    msr spsel, #MODE_SP_ELX

	sub sp, sp, (FP_CONTEXT_SIZE)

    /*需要在跳转之前保存x30(lr)*/
    stp    x29, x30, [sp, #-16]!

    bl    _exception_entry
    bl    do_exception
    mov   x0, sp
    b     restore_context
ENDPROC(vector_sync_sp_el1)

ENTRY(vector_irq_sp_el1)
    /*异常产生时，SPSel为1，即SP为SP_EL1，此处需要显示的将SP切换为SP_ELx。*/
    msr spsel, #MODE_SP_ELX

	sub sp, sp, (FP_CONTEXT_SIZE)

    /*需要在跳转之前保存x30(lr)*/
    stp    x29, x30, [sp, #-16]!

    bl    _exception_entry
    bl    do_irq
    mov   x0, sp
    b     restore_context
ENDPROC(vector_irq_sp_el1)


ENTRY(vector_sync_aarch64)
    switch_to_vm_sysStack

    bl _exception_entry

    /*获取异常类型EC*/
    mrs x29, esr_el1
    lsr x29, x29, #ESR_EC_SHIFT
    and x29, x29, #ESR_EC_MASK
    cmp x29, #EC_AARCH64_SVC
    b.ne .non_EL0_SVC

    /*
    *检查是否是用户态的系统调用(即是否是在EL0执行的SVC)，传参寄存器x0-x7可能保存着参数，
    *所以此处不能使用传参寄存器x0-x7。
    *
    *考虑到SSK是运行在EL1的，那么更低级别的EL只能是EL0。
    */
    mrs x29, CurrentEL
    cmp x29, #(SPSR_EL_EL1 << SPSR_EL_SHIFT)
    b.ne .non_EL0_SVC

    b do_syscall

.non_EL0_SVC:
    bl    do_exception
    mov   x0, sp
    b     restore_context
ENDPROC(vector_sync_aarch64)

ENTRY(vector_irq_aarch64)
    switch_to_vm_sysStack

    bl    _exception_entry
    bl    do_irq
    mov   x0, sp
    b     restore_context
ENDPROC(vector_irq_aarch64)
