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

#ifndef __ARCH_AARCH64_ASM_FAULT_RECOVERY_H__
#define __ARCH_AARCH64_ASM_FAULT_RECOVERY_H__

#include <fault_recovery_encoding.h>

#if defined(__ASSEMBLY__) || defined(__ASSEMBLER__)

	.irp	slot,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30
	.equ	.L__fixslot_x\slot, \slot
	.equ	.L__fixslot_w\slot, \slot
	.endr
	.equ	.L__fixslot_xzr, FAULT_RECOVERY_SLOT_NONE
	.equ	.L__fixslot_wzr, FAULT_RECOVERY_SLOT_NONE

#define __FAULT_RECOVERY_SLOT_PACK(reg, mask, shift) \
	(((.L__fixslot_##reg) << (shift)) & (mask))

#define FAULT_RECOVERY_EMIT_RAW(insn, resume, action, data)	\
	.pushsection	__fault_recovery, "a";		\
	.balign		4;				\
	.long		((insn) - .);			\
	.long		((resume) - .);			\
	.short		(action);			\
	.short		(data);				\
	.popsection;

#define FAULT_RECOVERY_EMIT_RESUME(insn, resume) \
	FAULT_RECOVERY_EMIT_RAW(insn, resume, FAULT_RECOVERY_ACTION_RESUME, 0)

#define FAULT_RECOVERY_EMIT_ERR_CLEAR(insn, resume, status_reg, clear_reg)				      \
	FAULT_RECOVERY_EMIT_RAW(insn, resume, FAULT_RECOVERY_ACTION_ERRNO_AND_CLEAR,			      \
				__FAULT_RECOVERY_SLOT_PACK(status_reg, FAULT_RECOVERY_STATUS_MASK, FAULT_RECOVERY_STATUS_SHIFT) | \
				__FAULT_RECOVERY_SLOT_PACK(clear_reg, FAULT_RECOVERY_CLEAR_MASK, FAULT_RECOVERY_CLEAR_SHIFT))

#define FAULT_RECOVERY_EMIT_ERR(insn, resume, status_reg) \
	FAULT_RECOVERY_EMIT_ERR_CLEAR(insn, resume, status_reg, wzr)

#endif /* __ASSEMBLY__ || __ASSEMBLER__ */

#endif /* __ARCH_AARCH64_ASM_FAULT_RECOVERY_H__ */
