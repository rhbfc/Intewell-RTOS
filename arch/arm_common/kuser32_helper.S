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

.section .rodata
	.align	5
	.globl	__kuser_helper_start
__kuser_helper_start:

__kuser_cmpxchg64:			// 0xffff0f60
	.inst	0xe92d00f0		//	push		{r4, r5, r6, r7}
	.inst	0xe1c040d0		//	ldrd		r4, r5, [r0]
	.inst	0xe1c160d0		//	ldrd		r6, r7, [r1]
	.inst	0xe1b20f9f		// 1:	ldrexd		r0, r1, [r2]
	.inst	0xe0303004		//	eors		r3, r0, r4
	.inst	0x00313005		//	eoreqs		r3, r1, r5
	.inst	0x01a23e96		//	stlexdeq	r3, r6, [r2]
	.inst	0x03330001		//	teqeq		r3, #1
	.inst	0x0afffff9		//	beq		1b
	.inst	0xf57ff05b		//	dmb		ish
	.inst	0xe2730000		//	rsbs		r0, r3, #0
	.inst	0xe8bd00f0		//	pop		{r4, r5, r6, r7}
	.inst	0xe12fff1e		//	bx		lr

	.align	5
__kuser_memory_barrier:			// 0xffff0fa0
	.inst	0xf57ff05b		//	dmb		ish
	.inst	0xe12fff1e		//	bx		lr

	.align	5
__kuser_cmpxchg:			// 0xffff0fc0
	.inst	0xe1923f9f		// 1:	ldrex		r3, [r2]
	.inst	0xe0533000		//	subs		r3, r3, r0
	.inst	0x01823f91		//	stlexeq		r3, r1, [r2]
	.inst	0x03330001		//	teqeq		r3, #1
	.inst	0x0afffffa		//	beq		1b
	.inst	0xf57ff05b		//	dmb		ish
	.inst	0xe2730000		//	rsbs		r0, r3, #0
	.inst	0xe12fff1e		//	bx		lr


	.align	5
__kuser_get_tls:			// 0xffff0fe0
	.inst	0xee1d0f70		//	mrc		p15, 0, r0, c13, c0, 3
	.inst	0xe12fff1e		//	bx		lr
	.rep	5
	.word	0
	.endr

__kuser_helper_version:			// 0xffff0ffc
	.word	((__kuser_helper_end - __kuser_helper_start) >> 5)
	.globl	__kuser_helper_end
__kuser_helper_end:
