/*
 * Copyright (c) 2013-2026, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ARCH_HELPERS_H
#define ARCH_HELPERS_H

/************************头文件********************************/
#include <system/macros.h>
#include <system/types.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

// clang-format off

#ifndef ASM_USE

/**********************************************************************
 * Macros which create inline functions to read or write CPU system
 * registers
 *********************************************************************/

#define _DEFINE_SYSREG_READ_FUNC(_name, _reg_name)		\
static inline unsigned long read_ ## _name(void)			\
{								\
	unsigned long v;						\
	__asm__ volatile ("mrs %0, " #_reg_name : "=r" (v));	\
	return v;						\
}

#define _DEFINE_SYSREG_WRITE_FUNC(_name, _reg_name)			\
static inline void write_ ## _name(unsigned long v)			\
{									\
	__asm__ volatile ("msr " #_reg_name ", %0" : : "r" (v));	\
}

#define SYSREG_WRITE_CONST(reg_name, v)				\
	__asm__ volatile ("msr " #reg_name ", %0" : : "i" (v))

/* Define read function for system register */
#define DEFINE_SYSREG_READ_FUNC(_name) 			\
	_DEFINE_SYSREG_READ_FUNC(_name, _name)

/* Define read & write function for system register */
#define DEFINE_SYSREG_RW_FUNCS(_name)			\
	_DEFINE_SYSREG_READ_FUNC(_name, _name)		\
	_DEFINE_SYSREG_WRITE_FUNC(_name, _name)

/* Define read & write function for renamed system register */
#define DEFINE_RENAME_SYSREG_RW_FUNCS(_name, _reg_name)	\
	_DEFINE_SYSREG_READ_FUNC(_name, _reg_name)	\
	_DEFINE_SYSREG_WRITE_FUNC(_name, _reg_name)

/* Define read function for renamed system register */
#define DEFINE_RENAME_SYSREG_READ_FUNC(_name, _reg_name)	\
	_DEFINE_SYSREG_READ_FUNC(_name, _reg_name)

/* Define write function for renamed system register */
#define DEFINE_RENAME_SYSREG_WRITE_FUNC(_name, _reg_name)	\
	_DEFINE_SYSREG_WRITE_FUNC(_name, _reg_name)

/**********************************************************************
 * Macros to create inline functions for system instructions
 *********************************************************************/

/* Define function for simple system instruction */
#define DEFINE_SYSOP_FUNC(_op)				\
static inline void _op(void)				\
{							\
	__asm__ (#_op);					\
}

/* Define function for system instruction with register parameter */
#define DEFINE_SYSOP_PARAM_FUNC(_op)			\
static inline void _op(unsigned long v)			\
{							\
	 __asm__ (#_op "  %0" : : "r" (v));		\
}

/* Define function for system instruction with type specifier */
#define DEFINE_SYSOP_TYPE_FUNC(_op, _type)		\
static inline void _op ## _type(void)			\
{							\
	__asm__ (#_op " " #_type : : : "memory");			\
}

/* Define function for system instruction with register parameter */
#define DEFINE_SYSOP_TYPE_PARAM_FUNC(_op, _type)	\
static inline void _op ## _type(unsigned long v)		\
{							\
	 __asm__ (#_op " " #_type ", %0" : : "r" (v));	\
}

// clang-format on

/*******************************************************************************
 * System register accessor prototypes
 ******************************************************************************/
DEFINE_SYSREG_READ_FUNC (midr_el1)
DEFINE_SYSREG_READ_FUNC (mpidr_el1)
DEFINE_SYSREG_READ_FUNC (id_aa64mmfr0_el1)
DEFINE_SYSREG_READ_FUNC (id_aa64mmfr1_el1)

DEFINE_SYSREG_RW_FUNCS (scr_el3)
DEFINE_SYSREG_RW_FUNCS (hcr_el2)

DEFINE_SYSREG_RW_FUNCS (vbar_el1)
DEFINE_SYSREG_RW_FUNCS (vbar_el2)
DEFINE_SYSREG_RW_FUNCS (vbar_el3)

DEFINE_SYSREG_RW_FUNCS (sctlr_el1)
DEFINE_SYSREG_RW_FUNCS (sctlr_el2)
DEFINE_SYSREG_RW_FUNCS (sctlr_el3)

DEFINE_SYSREG_RW_FUNCS (actlr_el1)
DEFINE_SYSREG_RW_FUNCS (actlr_el2)
DEFINE_SYSREG_RW_FUNCS (actlr_el3)

DEFINE_SYSREG_RW_FUNCS (esr_el1)
DEFINE_SYSREG_RW_FUNCS (esr_el2)
DEFINE_SYSREG_RW_FUNCS (esr_el3)

DEFINE_SYSREG_RW_FUNCS (afsr0_el1)
DEFINE_SYSREG_RW_FUNCS (afsr0_el2)
DEFINE_SYSREG_RW_FUNCS (afsr0_el3)

DEFINE_SYSREG_RW_FUNCS (afsr1_el1)
DEFINE_SYSREG_RW_FUNCS (afsr1_el2)
DEFINE_SYSREG_RW_FUNCS (afsr1_el3)

DEFINE_SYSREG_RW_FUNCS (far_el1)
DEFINE_SYSREG_RW_FUNCS (far_el2)
DEFINE_SYSREG_RW_FUNCS (far_el3)

DEFINE_SYSREG_RW_FUNCS (mair_el1)
DEFINE_SYSREG_RW_FUNCS (mair_el2)
DEFINE_SYSREG_RW_FUNCS (mair_el3)

DEFINE_SYSREG_RW_FUNCS (amair_el1)
DEFINE_SYSREG_RW_FUNCS (amair_el2)
DEFINE_SYSREG_RW_FUNCS (amair_el3)

DEFINE_SYSREG_READ_FUNC (rvbar_el1)
DEFINE_SYSREG_READ_FUNC (rvbar_el2)
DEFINE_SYSREG_READ_FUNC (rvbar_el3)

DEFINE_SYSREG_RW_FUNCS (rmr_el1)
DEFINE_SYSREG_RW_FUNCS (rmr_el2)
DEFINE_SYSREG_RW_FUNCS (rmr_el3)

DEFINE_SYSREG_RW_FUNCS (tcr_el1)
DEFINE_SYSREG_RW_FUNCS (tcr_el2)
DEFINE_SYSREG_RW_FUNCS (tcr_el3)

DEFINE_SYSREG_RW_FUNCS (ttbr0_el1)
DEFINE_SYSREG_RW_FUNCS (ttbr0_el2)
DEFINE_SYSREG_RW_FUNCS (ttbr0_el3)

DEFINE_SYSREG_RW_FUNCS (ttbr1_el1)

DEFINE_SYSREG_RW_FUNCS (vttbr_el2)

DEFINE_SYSREG_RW_FUNCS (cptr_el2)
DEFINE_SYSREG_RW_FUNCS (cptr_el3)

DEFINE_SYSREG_RW_FUNCS (cpacr_el1)
DEFINE_SYSREG_RW_FUNCS (cntfrq_el0)
DEFINE_SYSREG_RW_FUNCS (cnthp_ctl_el2)
DEFINE_SYSREG_RW_FUNCS (cnthp_tval_el2)
DEFINE_SYSREG_RW_FUNCS (cnthp_cval_el2)
DEFINE_SYSREG_RW_FUNCS (cntps_ctl_el1)
DEFINE_SYSREG_RW_FUNCS (cntps_tval_el1)
DEFINE_SYSREG_RW_FUNCS (cntps_cval_el1)
DEFINE_SYSREG_RW_FUNCS (cntp_ctl_el0)
DEFINE_SYSREG_RW_FUNCS (cntp_tval_el0)
DEFINE_SYSREG_RW_FUNCS (cntp_cval_el0)
DEFINE_SYSREG_READ_FUNC (cntpct_el0)
DEFINE_SYSREG_RW_FUNCS (cnthctl_el2)
DEFINE_SYSREG_RW_FUNCS (cntkctl_el1)
DEFINE_SYSREG_RW_FUNCS (cntv_ctl_el0)
DEFINE_SYSREG_RW_FUNCS (cntv_tval_el0)

DEFINE_SYSREG_RW_FUNCS (vtcr_el2)

DEFINE_SYSREG_RW_FUNCS (tpidr_el3)
DEFINE_SYSREG_RW_FUNCS (tpidr_el1) // cpuid
DEFINE_SYSREG_RW_FUNCS (tpidr_el0) // tls

DEFINE_SYSREG_RW_FUNCS (cntvoff_el2)

DEFINE_SYSREG_RW_FUNCS (vpidr_el2)
DEFINE_SYSREG_RW_FUNCS (vmpidr_el2)

DEFINE_SYSREG_READ_FUNC (isr_el1)

DEFINE_SYSREG_RW_FUNCS (mdcr_el2)
DEFINE_SYSREG_RW_FUNCS (mdcr_el3)
DEFINE_SYSREG_RW_FUNCS (hstr_el2)
DEFINE_SYSREG_RW_FUNCS (pmcr_el0)

DEFINE_SYSREG_RW_FUNCS (id_aa64dfr0_el1)
DEFINE_SYSREG_RW_FUNCS (mdscr_el1)
DEFINE_SYSREG_RW_FUNCS (contextidr_el1)
DEFINE_SYSREG_RW_FUNCS (osdlr_el1)
DEFINE_SYSREG_RW_FUNCS (oslar_el1)


DEFINE_SYSREG_RW_FUNCS (dbgbcr0_el1)
DEFINE_SYSREG_RW_FUNCS (dbgbcr1_el1)
DEFINE_SYSREG_RW_FUNCS (dbgbcr2_el1)
DEFINE_SYSREG_RW_FUNCS (dbgbcr3_el1)
DEFINE_SYSREG_RW_FUNCS (dbgbcr4_el1)
DEFINE_SYSREG_RW_FUNCS (dbgbcr5_el1)
DEFINE_SYSREG_RW_FUNCS (dbgbcr6_el1)
DEFINE_SYSREG_RW_FUNCS (dbgbcr7_el1)
DEFINE_SYSREG_RW_FUNCS (dbgbcr8_el1)
DEFINE_SYSREG_RW_FUNCS (dbgbcr9_el1)
DEFINE_SYSREG_RW_FUNCS (dbgbcr10_el1)
DEFINE_SYSREG_RW_FUNCS (dbgbcr11_el1)
DEFINE_SYSREG_RW_FUNCS (dbgbcr12_el1)
DEFINE_SYSREG_RW_FUNCS (dbgbcr13_el1)
DEFINE_SYSREG_RW_FUNCS (dbgbcr14_el1)
DEFINE_SYSREG_RW_FUNCS (dbgbcr15_el1)

DEFINE_SYSREG_RW_FUNCS (dbgwvr0_el1)
DEFINE_SYSREG_RW_FUNCS (dbgwvr1_el1)
DEFINE_SYSREG_RW_FUNCS (dbgwvr2_el1)
DEFINE_SYSREG_RW_FUNCS (dbgwvr3_el1)
DEFINE_SYSREG_RW_FUNCS (dbgwvr4_el1)
DEFINE_SYSREG_RW_FUNCS (dbgwvr5_el1)
DEFINE_SYSREG_RW_FUNCS (dbgwvr6_el1)
DEFINE_SYSREG_RW_FUNCS (dbgwvr7_el1)
DEFINE_SYSREG_RW_FUNCS (dbgwvr8_el1)
DEFINE_SYSREG_RW_FUNCS (dbgwvr9_el1)
DEFINE_SYSREG_RW_FUNCS (dbgwvr10_el1)
DEFINE_SYSREG_RW_FUNCS (dbgwvr11_el1)
DEFINE_SYSREG_RW_FUNCS (dbgwvr12_el1)
DEFINE_SYSREG_RW_FUNCS (dbgwvr13_el1)
DEFINE_SYSREG_RW_FUNCS (dbgwvr14_el1)
DEFINE_SYSREG_RW_FUNCS (dbgwvr15_el1)

DEFINE_SYSREG_RW_FUNCS (dbgbvr0_el1)
DEFINE_SYSREG_RW_FUNCS (dbgbvr1_el1)
DEFINE_SYSREG_RW_FUNCS (dbgbvr2_el1)
DEFINE_SYSREG_RW_FUNCS (dbgbvr3_el1)
DEFINE_SYSREG_RW_FUNCS (dbgbvr4_el1)
DEFINE_SYSREG_RW_FUNCS (dbgbvr5_el1)
DEFINE_SYSREG_RW_FUNCS (dbgbvr6_el1)
DEFINE_SYSREG_RW_FUNCS (dbgbvr7_el1)
DEFINE_SYSREG_RW_FUNCS (dbgbvr8_el1)
DEFINE_SYSREG_RW_FUNCS (dbgbvr9_el1)
DEFINE_SYSREG_RW_FUNCS (dbgbvr10_el1)
DEFINE_SYSREG_RW_FUNCS (dbgbvr11_el1)
DEFINE_SYSREG_RW_FUNCS (dbgbvr12_el1)
DEFINE_SYSREG_RW_FUNCS (dbgbvr13_el1)
DEFINE_SYSREG_RW_FUNCS (dbgbvr14_el1)
DEFINE_SYSREG_RW_FUNCS (dbgbvr15_el1)

DEFINE_SYSREG_RW_FUNCS (dbgwcr0_el1)
DEFINE_SYSREG_RW_FUNCS (dbgwcr1_el1)
DEFINE_SYSREG_RW_FUNCS (dbgwcr2_el1)
DEFINE_SYSREG_RW_FUNCS (dbgwcr3_el1)
DEFINE_SYSREG_RW_FUNCS (dbgwcr4_el1)
DEFINE_SYSREG_RW_FUNCS (dbgwcr5_el1)
DEFINE_SYSREG_RW_FUNCS (dbgwcr6_el1)
DEFINE_SYSREG_RW_FUNCS (dbgwcr7_el1)
DEFINE_SYSREG_RW_FUNCS (dbgwcr8_el1)
DEFINE_SYSREG_RW_FUNCS (dbgwcr9_el1)
DEFINE_SYSREG_RW_FUNCS (dbgwcr10_el1)
DEFINE_SYSREG_RW_FUNCS (dbgwcr11_el1)
DEFINE_SYSREG_RW_FUNCS (dbgwcr12_el1)
DEFINE_SYSREG_RW_FUNCS (dbgwcr13_el1)
DEFINE_SYSREG_RW_FUNCS (dbgwcr14_el1)
DEFINE_SYSREG_RW_FUNCS (dbgwcr15_el1)

#define write_cntfrq(v)     write_cntfrq_el0 (v)
#define write_cnthctl(v)    write_cnthctl_el2 (v)
#define write_cntkctl(v)    write_cntkctl_el1 (v)
#define write_cnthp_ctl(v)  write_cnthp_ctl_el2 (v)
#define write_cnthp_tval(v) write_cnthp_tval_el2 (v)
#define write_cntp_ctl(v)   write_cntp_ctl_el0 (v)
#define write_cntp_tval(v)  write_cntp_tval_el0 (v)
#define write_cntv_ctl(v)   write_cntv_ctl_el0 (v)
#define write_cntv_tval(v)  write_cntv_tval_el0 (v)
#define write_cntp_cval(v)  write_cntp_cval_el0 (v)

#define read_cntfrq()     read_cntfrq_el0 ()
#define read_cnthctl()    read_cnthctl_el2 ()
#define read_cntkctl()    read_cntkctl_el1 ()
#define read_cnthp_ctl()  read_cnthp_ctl_el2 ()
#define read_cnthp_tval() read_cnthp_tval_el2 ()
#define read_cntp_ctl()   read_cntp_ctl_el0 ()
#define read_cntp_tval()  read_cntp_tval_el0 ()
#define read_cntv_ctl()   read_cntv_ctl_el0 ()
#define read_cntv_tval()  read_cntv_tval_el0 ()
#define read_cntp_cval()  read_cntp_cval_el0 ()
#define read_cntpct()     read_cntpct_el0 ()

DEFINE_SYSOP_FUNC (wfi)
DEFINE_SYSOP_FUNC (wfe)
DEFINE_SYSOP_FUNC (sev)
DEFINE_SYSOP_TYPE_FUNC (dsb, sy)
DEFINE_SYSOP_TYPE_FUNC (dmb, sy)
DEFINE_SYSOP_TYPE_FUNC (dmb, st)
DEFINE_SYSOP_TYPE_FUNC (dmb, ld)
DEFINE_SYSOP_TYPE_FUNC (dsb, ish)
DEFINE_SYSOP_TYPE_FUNC (dsb, osh)
DEFINE_SYSOP_TYPE_FUNC (dsb, nsh)
DEFINE_SYSOP_TYPE_FUNC (dsb, ishst)
DEFINE_SYSOP_TYPE_FUNC (dsb, oshst)
DEFINE_SYSOP_TYPE_FUNC (dmb, oshld)
DEFINE_SYSOP_TYPE_FUNC (dmb, oshst)
DEFINE_SYSOP_TYPE_FUNC (dmb, osh)
DEFINE_SYSOP_TYPE_FUNC (dmb, nshld)
DEFINE_SYSOP_TYPE_FUNC (dmb, nshst)
DEFINE_SYSOP_TYPE_FUNC (dmb, nsh)
DEFINE_SYSOP_TYPE_FUNC (dmb, ishld)
DEFINE_SYSOP_TYPE_FUNC (dmb, ishst)
DEFINE_SYSOP_TYPE_FUNC (dmb, ish)
DEFINE_SYSOP_FUNC (isb)

DEFINE_SYSOP_TYPE_PARAM_FUNC(tlbi, vaae1is)
DEFINE_SYSOP_TYPE_PARAM_FUNC(tlbi, vaale1is)
DEFINE_SYSOP_TYPE_PARAM_FUNC(tlbi, vae2is)
DEFINE_SYSOP_TYPE_PARAM_FUNC(tlbi, vale2is)
DEFINE_SYSOP_TYPE_PARAM_FUNC(tlbi, vae3is)
DEFINE_SYSOP_TYPE_PARAM_FUNC(tlbi, vale3is)

DEFINE_SYSOP_TYPE_FUNC(tlbi, alle1)
DEFINE_SYSOP_TYPE_FUNC(tlbi, alle1is)
DEFINE_SYSOP_TYPE_FUNC(tlbi, alle2)
DEFINE_SYSOP_TYPE_FUNC(tlbi, alle2is)
DEFINE_SYSOP_TYPE_FUNC(tlbi, alle3)
DEFINE_SYSOP_TYPE_FUNC(tlbi, alle3is)
DEFINE_SYSOP_TYPE_FUNC(tlbi, vmalle1)

#endif /* ASM_USE */

/************************类型定义******************************/

/************************接口声明******************************/

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* ARCH_HELPERS_H */
