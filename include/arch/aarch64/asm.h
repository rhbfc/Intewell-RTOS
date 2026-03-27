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

/*
 * @file： asm.h
 * @brief：
 *	    <li>提供arm汇编相关的宏定义。</li>
 */

#ifndef _ASM_H
#define _ASM_H

/************************头文件********************************/
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/************************宏定义********************************/

// clang-format off

#ifdef ASM_USE

#define ALIGN .align 4

#define ENTRY(name)                             \
  .globl name;                                  \
  ALIGN;                                        \
  name:

#define GLOBAL(name)                            \
  .globl name;                                  \
  name:
  
#define END(name) \
  .size name, .-name
  
#define ENDPROC(name) \
  .type name, %function; \
  END(name)

/*
  * Helper macro to generate the best mov/movk combinations according
  * the value to be moved. The 16 bits from '_shift' are tested and
  * if not zero, they are moved into '_reg' without affecting
  * other bits.
  */
.macro _mov_imm16 _reg, _val, _shift
  .if (\_val >> \_shift) & 0xffff
    .if (\_val & (1 << \_shift - 1))
      movk	\_reg, (\_val >> \_shift) & 0xffff, LSL \_shift
    .else
      mov	\_reg, \_val & (0xffff << \_shift)
    .endif
  .endif
.endm

/*
  * Helper macro to load arbitrary values into 32 or 64-bit registers
  * which generates the best mov/movk combinations. Many base addresses
  * are 64KB aligned the macro will eliminate updating bits 15:0 in
  * that case
  */
.macro mov_imm _reg, _val
  .if (\_val) == 0
    mov	\_reg, #0
  .else
    _mov_imm16	\_reg, (\_val), 0
    _mov_imm16	\_reg, (\_val), 16
    _mov_imm16	\_reg, (\_val), 32
    _mov_imm16	\_reg, (\_val), 48
  .endif
.endm

/*
 * Branch according to exception level
 */
.macro    switch_el, xreg, el3_label, el2_label, el1_label
    mrs    \xreg, CurrentEL
    cmp    \xreg, 0xc
    b.eq    \el3_label
    cmp    \xreg, 0x8
    b.eq    \el2_label
    cmp    \xreg, 0x4
    b.eq    \el1_label
.endm

.macro restore_fpcr state, tmp
    /*
    * Writes to fpcr may be self-synchronising, so avoid restoring
    * the register if it hasn't changed.
    */
    mrs    \tmp, fpcr
    cmp    \tmp, \state
    b.eq    166f
    msr    fpcr, \state
166:
.endm

.macro    dcache_line_size  reg, tmp
    mrs    \tmp, ctr_el0
    
    /* 获取数据cache line words 大小的Log2，即ctr_el0[19-16]。*/
    ubfx    \tmp, \tmp, #16, #4

    /* 一个word是4个字节*/
    mov    \reg, #4

    /* 获取数据cache line的大小*/   
    lsl    \reg, \reg, \tmp
.endm

.macro    icache_line_size  reg, tmp
    mrs    \tmp, ctr_el0
    and    \tmp, \tmp, #0xf
    mov    \reg, #4
    lsl    \reg, \reg, \tmp
.endm

/*
*从tpidr_el1中快速获取cpuid。
*
*xa表示宏里面要使用的寄存器。
*/
.macro  get_cpuid_quickly, xa

    /* 获取CPU号 */
    mrs \xa, tpidr_el1

.endm

/*
*重新设置SP_EL1为系统栈。
*
*xa, xb表示宏里面要使用的寄存器。
*/
.globl  running_task_stack
.macro  set_sp_el1_sysstack_per_cpu, xa, xb

    /* 获取CPU号 */
    get_cpuid_quickly \xa

    ldr \xb, =running_task_stack

    add \xb, \xb, \xa, lsl #3
    ldr \xa, [\xb]

    mov sp, \xa
.endm

#define OFFSET	304
/*
*xa  表示指向异常上下文T_RawExceptionContext的指针。
*/
.macro  restore_raw_excContext, xa
    /*禁止中断，否则elr和spsr的值会被改变。*/
    msr daifset, #DAIF_IRQ_BIT

    mov x30, \xa
    
#ifdef _HARD_FLOAT_
		mov x0, x30
		add x0, x0, #S_FP_CONTEXT_START
		
		ldp q0, q1, [x0], #32	
		ldp q2, q3, [x0], #32
		ldp q4, q5, [x0], #32	
		ldp q6, q7, [x0], #32	
		ldp q8, q9, [x0], #32	
		ldp q10, q11, [x0], #32 
		ldp q12, q13, [x0], #32 
		ldp q14, q15, [x0], #32 	
		ldp q16, q17, [x0], #32 
		ldp q18, q19, [x0], #32 
		ldp q20, q21, [x0], #32 
		ldp q22, q23, [x0], #32 
		ldp q24, q25, [x0], #32 
		ldp q26, q27, [x0], #32 
		ldp q28, q29, [x0], #32 
		ldp q30, q31, [x0], #32 
		
		ldp x1, x2, [x0]
		MSR FPSR, x1
		MSR FPCR, x2
#endif

    /* 恢复cpacr */
    ldr x4, [x30, #S_CPACR]
    msr cpacr_el1, x4

    /* 获取spsr */
    ldr x20, [x30, #S_SPSR]

    and x3, x20, #SPSR_SS
    mrs x2, mdscr_el1 
    lsr x3, x3, #SPSRSS_SHIFT_MINUS_MDSCRSS_SHIFT
    orr x2, x2, x3
    msr mdscr_el1, x2 	/* 仅SS位为1的 使能mdscr_SS */

    /* 解锁OS LOCK */
    mov x3, #0
    msr oslar_el1, x3 

    /*注意:在将x20写入spsr之前，不能修改x20。*/

    lsr x3, x20, #SPSR_EL_SHIFT
    and x3, x3, #SPSR_EL_MASK
    cmp x3, #SPSR_EL_EL0

    /* 获取esr sp*/
    ldr x3, [x30, #S_ESR]
    ldr x2, [x30, #S_SP]
    
    b.ne 9994f
    
	/* x19用来保存赋给SPSR.SS的值，
	   由于该宏不会返回，所以不用考虑保存恢复<被调者保存寄存器> */  	
	mov x19, #0

	/* 对SPSR的SS位赋值 */
	orr x20, x20, x19	
	
    /*用户态EL0产生的异常，更新用户态栈指针sp_el0*/
    msr sp_el0, x2

    /*恢复到EL0模式前，需要将SP_EL1更新为当前CPU对应的系统栈，在EL0产生异常时可直接使用SP_EL1*/
    set_sp_el1_sysstack_per_cpu x1 x2

    b 9995f

9994:
    /*
    *核心态EL1产生的异常，更新SP_EL1。
    *当前sp即是SP_EL1。
    */
    mov sp, x2
    
9995:   
    /*加载elr、x0*/
    ldr    x2, [x30, #S_ELR]
    ldr    x0, [x30, #S_X0]
    switch_el x11, 9993f, 9992f, 9991f

9993:    
    msr    elr_el3, x2
    msr    spsr_el3, x20
    b    9990f

9992:    
    msr    elr_el2, x2
    msr    spsr_el2, x20  
    b    9990f

9991:    
    msr    elr_el1, x2
    msr    spsr_el1, x20        
    b    9990f

/*
 * 对于EL0异常，恢复x1-x30，在恢复完通用寄存器后，核心态SP_EL1栈指针也对应的恢复了。
 * 对于EL1异常，恢复x1-x30，在恢复完通用寄存器后，栈指针也对应的恢复了。
 */
9990:
    /* x30指向x1 */
    add x30, x30, #S_X1

    /*加载x1、x2*/
    ldp    x1, x2, [x30],#16
    
    ldp    x3, x4, [x30],#16
    ldp    x5, x6, [x30],#16
    ldp    x7, x8, [x30],#16
    ldp    x9, x10, [x30],#16
    ldp    x11, x12, [x30],#16
    ldp    x13, x14, [x30],#16
    ldp    x15, x16, [x30],#16
    ldp    x17, x18, [x30],#16
    ldp    x19, x20, [x30],#16
    ldp    x21, x22, [x30],#16
    ldp    x23, x24, [x30],#16
    ldp    x25, x26, [x30],#16
    ldp    x27, x28, [x30],#16
    ldp    x29, x30, [x30]
  
    dsb    sy
    isb
    eret
.endm    

#endif /* ASM_USE */

// clang-format on

/************************类型定义******************************/

/************************接口声明******************************/

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _ASM_H */
