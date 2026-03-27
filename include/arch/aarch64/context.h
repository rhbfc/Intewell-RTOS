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
 * @file： context.h
 * @brief：
 *	    <li>提供上下文相关的宏定义。</li>
 */

#ifndef _CONTEXT_H
#define _CONTEXT_H

/************************头文件********************************/
#include <system/types.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/************************宏定义********************************/

/* 上下文寄存器偏移量 */

/************************类型定义******************************/

#ifndef ASM_USE


#ifdef _HARD_FLOAT_

/* 
*全浮点寄存器上下文定义
*
*异常和中断上下文中不包括浮点上下文，显示的提供一对接口fppSaveContext()、fppRestoreContext()
*来保存和恢复所有的浮点寄存器，即所有浮点寄存器、FPCR、FPSR。异常和中断处理中如果
*使用了浮点操作，在进行浮点操作前一定要调用fppSaveContext()来保存浮点上下文，浮点操作
*完成后一定要调用fppRestoreContext ()来恢复浮点上下文。否则，在返回到之前的中断或者异常
*的点执行时，浮点操作会不正确。
*
*由于考虑到对SMID操作的支持，所以保存的是quadword registers，即V0-V31。
*
*__uint128_t是64位模式下，gcc的内置16字节类型。
*
*为了使用配对加载和存储功能，需要保证32字节对齐。
*/
typedef struct
{
    __uint128_t  vRegs[32];
    u64 fpsr;
    u64 fpcr;
    //u64  cpacr; /*Architectural Feature Access Control Register，控制浮点操作的使能和禁止。*/
}T_FPP_Context __attribute__ ((__aligned__ (16)));

#endif  /* _HARD_FLOAT_ */

/*
*异常上下定义 。
*
*为了使用配对加载和存储功能，需要保证是16字节对齐。
*/
struct arch_context
{   u64  err_sp;
    u64  ori_x0;
    u64  type;  /*上下文类型*/
    u64  cpacr; /*Architectural Feature Access Control Register，控制浮点操作的使能和禁止。*/
    u64 vector;
    u64 cpsr; /* Saved Program Status Register */
    u64 esr; /*Exception Syndrome Register, synchronous和SError异常时，此寄存器才有意义。*/
    u64 sp;
    u64 elr;/*Exception Link Register, When taking an exception to ELx, holds the address to return to.*/
    u64 regs[31]; /*x0-x30*/
#ifdef _HARD_FLOAT_    
    T_FPP_Context fpContext;
#endif
}__attribute__ ((__aligned__ (16)));

/* 中断/异常上下文类型定义 */
typedef struct arch_context arch_int_context_t;
typedef struct arch_context arch_exception_context_t;

/*------------------Task的切换上下文定义--------------------*/
/* ARM的寄存器上下文定义*/
typedef struct
{
#ifdef _HARD_FLOAT_
    u64 d8;
    u64 d9;
    u64 d10;
    u64 d11;
    u64 d12;
    u64 d13;
    u64 d14;
    u64 d15;
    u64 fpsr;
    u64 fpcr;
#endif
    u64 x19;
    u64 x20;
    u64 x21;
    u64 x22;
    u64 x23;
    u64 x24;
    u64 x25;
    u64 x26;
    u64 x27;
    u64 x28;
    u64 fp;
    u64 pc;
    u64 sp;
    u64 cpsr;
    u64 tls;    /* 用于存放posix线程上下文 */
    u64 ttbr0;  /* 用户进程的进程空间 */
} T_TBSP_TaskContext;

void *exception_sp_get (struct arch_context *context);
#endif  /* ASM_USE */

/************************接口声明******************************/

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _CONTEXT_H */
