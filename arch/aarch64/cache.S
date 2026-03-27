/*
 * Copyright (c) 2013-2022, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/************************头 文 件******************************/

#define ASM_USE
#include <asm.h>
#include <cpu.h>

/************************宏 定 义******************************/

/*
 * 按虚拟地址范围执行数据 Cache 维护操作。
 * x0 为起始地址，x1 为长度，op 为 dc 指令操作码。
 */
.macro do_dcache_maintenance_by_mva op
	/* 长度为 0 时直接返回。 */
	cbz	x1, exit_loop_\op

	/* 获取数据 Cache 的 cache line 大小，单位为字节。 */
	dcache_line_size x2, x3

	/* 计算结束地址。 */
	add	x1, x0, x1

	/* 获取 cache line 大小的掩码。 */
	sub	x3, x2, #1

	/* 将起始地址按 cache line 大小向下对齐。 */
	bic	x0, x0, x3

	/* 以 cache line 为单位执行指定操作。 */
loop_\op:
	dc	\op, x0
	add	x0, x0, x2
	cmp	x0, x1
	b.lo    loop_\op
	dsb	sy
exit_loop_\op:
	ret
.endm

/************************类型定义******************************/

/************************外部声明******************************/

/************************前向声明******************************/

.globl	dcsw_op_louis
.globl	dcsw_op_all
.globl	dcsw_op_level1
.globl	dcsw_op_level2
.globl	dcsw_op_level3

/************************模块变量******************************/

/************************函数实现******************************/

/**
 * @brief
 *    使能指令 Cache
 * @param[in] 无
 * @retval 无
 */
ENTRY(cache_icache_enable)
	mrs x0, sctlr_el1
	orr x0, x0, #(1 << 12)   /* I */
	msr sctlr_el1, x0
	dsb    sy
	isb
	ret
ENDPROC(cache_icache_enable)

/**
 * @brief
 *    禁止指令 Cache
 * @param[in] 无
 * @retval 无
 */
ENTRY(cache_icache_disable)
	mrs x0, sctlr_el1
	bic x0, x0, #(1 << 12)   /* I */
	msr sctlr_el1, x0
	dsb    sy
	isb
	ret
ENDPROC(cache_icache_disable)

/**
 * @brief
 *    使能数据 Cache
 * @param[in] 无
 * @retval 无
 */
ENTRY(cache_dcache_enable)
	mrs x0, sctlr_el1
	orr x0, x0, #(1 << 2)   /* C */
	msr sctlr_el1, x0
	dsb    sy
	isb
	ret
ENDPROC(cache_dcache_enable)

/**
 * @brief
 *    禁止数据 Cache
 * @param[in] 无
 * @retval 无
 */
ENTRY(cache_dcache_disable)
	mrs x0, sctlr_el1
	bic x0, x0, #(1 << 2)   /* C */
	msr sctlr_el1, x0
	dsb    sy
	isb
	ret
ENDPROC(cache_dcache_disable)

/**
 * @brief
 *    无效整个指令 Cache
 * @param[in] 无
 * @retval 无
 */
ENTRY(cache_icache_invalidate_all)
	ic ialluis
	dsb	   ish
	isb
	ret
ENDPROC(cache_icache_invalidate_all)

/**
 * @brief
 *    刷新整个数据 Cache，并将脏数据写回内存
 * @param[in] 无
 * @retval 无
 */
ENTRY(cache_dcache_flush_all)
    DMB    SY
    MRS    X0, CLIDR_EL1
    AND    W3, W0, #CLIDR_LOC
    LSR    W3, W3, #(CLIDR_LOC_SHIFT - 1)   /* get 2x LoC */
    CBZ    W3, 5f                           /* done */
    MOV    W10, #0                          /* W10 = 2x LoC */
    MOV    W8, #1                           /* W8 = constant 0b1 */
1:
    ADD    W2, W10, W10, LSR #1             /* W2 = 3x LoC */
    LSR    W1, W0, W2
    AND    W1, W1, #CLIDR_CTYPE_MASK        /* get cache type */
    CMP    W1, #CLIDR_CTYPE_D
    B.LT   4f                               /* skip if no or i-cache */
    MRS    X14, DAIF
    MSR    DAIFSet, #3
    MSR    CSSELR_EL1, X10                  /* select this cache */
    ISB                                     /* Synchronize change of CSSELR */
    MRS    X1, CCSIDR_EL1                   /* read CCSIDR */
    MSR    DAIF, X14
    AND    W2, W1, #CCSIDR_LINESIZE_MASK    /* W2 = log2(linelen)-4 */
    ADD    W2, W2, #4                       /* W2 = log2(linelen) */
    UBFX   W4, W1, #3, #10                  /* W4 = max way number/associativity */
    CLZ    W5, W4                           /* W5 = 32-log2(ways), bit position of way in DC operand */
    LSL    W9, W4, W5                       /* W9 = max way number, aligned to position in DC operand */
    LSL    W12, W8, W5                      /* W12 = amount to decrement way number per iteration */
2:
    UBFX   W7, W1, #13, #15                 /* W7 = max set number, right aligned */
    LSL    W7, W7, W2                       /* W7 = max set number, aligned to position in DC operand */
    LSL    W13, W8, W2                      /* W13 = amount to decrement set number per iteration */
3:
    ORR    W11, W10, W9                     /* W11 = combine way number and cache number */
    ORR    W11, W11, W7                     /* and set number for DC operand */
    DC     CSW, X11                         /* Do data cache operation by set and way */
    SUBS   W7, W7, W13                      /* Decrement set number */
    B.GE   3b
    SUBS   X9, X9, X12                      /* Decrement way number */
    B.GE   2b
4:
    ADD   W10, W10, #2                      /* Increment 2 x cache level */
    CMP   W3, W10
    DSB   SY
    B.GT  1b
5:
    MOV   X10, #0                           /* switch back to cache level 0 */
    MSR   CSSELR_EL1, X10                   /* select current cache level in csselr */
    DSB   SY
    ISB

    RET
ENDPROC(cache_dcache_flush_all)

/**
 * @brief
 *    无效整个数据 Cache
 * @param[in] 无
 * @retval 无
 */
ENTRY(cache_dcache_invalidate_all)
    DMB    SY
    MRS    X0, CLIDR_EL1
    AND    W3, W0, #CLIDR_LOC
    LSR    W3, W3, #(CLIDR_LOC_SHIFT - 1)    /* get 2x LoC */
    CBZ    W3, 5f                            /* done */
    MOV    W10, #0                           /* W10 = 2x LoC */
    MOV    W8, #1                            /* W8 = constant 0b1 */
1:
    ADD    W2, W10, W10, LSR #1              /* W2 = 3x LoC */
    LSR    W1, W0, W2
    AND    W1, W1, #CLIDR_CTYPE_MASK         /* get cache type */
    CMP    W1, #CLIDR_CTYPE_D
    B.LT   4f                                /* skip if no or i-cache */

    MRS    X14, DAIF
    MSR    DAIFSet, #3
    MSR    CSSELR_EL1, X10                   /* select this cache */
    ISB                                      /* Synchronize change of CSSELR */
    MRS    X1, CCSIDR_EL1                    /* read CCSIDR */
    MSR    DAIF, X14
    AND    W2, W1, #CCSIDR_LINESIZE_MASK     /* W2 = log2(linelen)-4 */
    ADD    W2, W2, #4                        /* W2 = log2(linelen) */
    UBFX   W4, W1, #3, #10                   /* W4 = max way number/associativity */
    CLZ    W5, W4                            /* W5 = 32-log2(ways), bit position of way in DC operand */
    LSL    W9, W4, W5                        /* W9 = max way number, aligned to position in DC operand */
    LSL    W12, W8, W5                       /* W12 = amount to decrement way number per iteration */
2:
    UBFX   W7, W1, #13, #15                  /* W7 = max set number, right aligned */
    LSL    W7, W7, W2                        /* W7 = max set number, aligned to position in DC operand */
    LSL    W13, W8, W2                       /* W13 = amount to decrement set number per iteration */
3:
    ORR    W11, W10, W9                      /* W11 = combine way number and cache number */
    ORR    W11, W11, W7                      /* and set number for DC operand */
    DC     ISW, X11                          /* Do data cache operation by set and way */
    SUBS   W7, W7, W13                       /* Decrement set number */
    B.GE   3b
    SUBS   X9, X9, X12                       /* Decrement way number */
    B.GE   2b
4:
    ADD    W10, W10, #2                      /* Increment 2 x cache level */
    CMP    W3, W10
    DSB    SY
    B.GT   1b
5:
    MOV    X10, #0                           /* switch back to cache level 0 */
    MSR    CSSELR_EL1, X10                   /* select current cache level in csselr */
    DSB    SY
    ISB

    RET
ENDPROC(cache_icache_invalidate_all)

/*
 * 按地址范围执行 clean + invalidate。
 * x0 为起始地址，x1 为长度。
 */
ENTRY(cache_dcache_clean)
	/* 刷新并无效指定虚拟地址范围内的数据到 PoC。 */
	do_dcache_maintenance_by_mva civac
ENDPROC(cache_dcache_clean)

/*
 * 按地址范围执行 clean。
 * x0 为起始地址，x1 为长度。
 */
ENTRY(cache_dcache_flush)
    /* 刷新指定虚拟地址范围内的数据到 PoC。 */
    do_dcache_maintenance_by_mva cvac
ENDPROC(cache_dcache_flush)

/*
 * 按地址范围执行 invalidate。
 * x0 为起始地址，x1 为长度。
 */
ENTRY(cache_dcache_invalidate)
    /* 无效指定虚拟地址范围内的数据到 PoC。 */
    do_dcache_maintenance_by_mva ivac
ENDPROC(cache_dcache_invalidate)

/**
 * @brief
 *    更新指令 Cache
 * @param[in] startAddr: 起始地址
 * @param[in] size: 地址范围长度，单位为字节
 * @retval 无
 * @note
 *    先刷新对应数据 Cache，再无效对应指令 Cache。
 */

ENTRY(cache_text_update)
    /* 获取数据 Cache 的 cache line 大小，单位为字节。 */
    dcache_line_size x2, x3

    /* 计算结束地址。 */
    add	x7, x0, x1

    /* 获取 cache line 大小掩码。 */
    sub	x3, x2, #0x1

    /* 将起始地址按 cache line 大小向下对齐。 */
    bic	x8, x0, x3

.dataCacheFlushLoop:
    /* 以 cache line 为单位将数据 Cache 刷新到 PoU。 */
    dc	cvau, x8

    add	x8, x8, x2
    cmp	x8, x7
    b.cc	.dataCacheFlushLoop

    dsb	sy

    /* 获取指令 Cache 的 cache line 大小，单位为字节。 */
    icache_line_size x2, x3

    /* 获取 cache line 大小掩码。 */
    sub	x3, x2, #0x1

    /* 将起始地址按 cache line 大小向下对齐。 */
    bic	x8, x0, x3

.instCacheInvLoop:
    /* 以 cache line 为单位无效指定范围的指令 Cache 到 PoU。 */
    ic 	ivau, x8

    add	x8, x8, x2
    cmp	x8, x7
    b.cc	.instCacheInvLoop

    dsb	sy

    /* 同步指令预取，避免后续继续执行旧指令。 */
    isb

    ret
ENDPROC(cache_text_update)

/*
 * 按 set/way 对指定层级范围内的数据 Cache 执行维护操作。
 *
 * do_dcsw_op 入口参数：
 * x0: 操作类型（0-2，对应 arch.h 中定义）
 * x3: 需要处理到的最后一层 Cache
 * x9: clidr_el1
 * x10: 起始 Cache 层级
 *
 * dcsw_op 宏会先根据 clidr_el1 计算 x3 和 x9，
 * 然后再跳转到 do_dcsw_op。
 */

.macro	dcsw_op shift, fw, ls
    mrs	x9, clidr_el1
    ubfx	x3, x9, \shift, \fw
    lsl	x3, x3, \ls
    mov	x10, xzr
    b	do_dcsw_op
.endm

ENTRY(do_dcsw_op)
	cbz	x3, exit
	adr	x14, dcsw_loop_table	/* compute inner loop address */
	add	x14, x14, x0, lsl #5	/* inner loop is 8x32-bit instructions */
#if ENABLE_BTI
	add	x14, x14, x0, lsl #2	/* inner loop is + "bti j" instruction */
#endif
	mov	x0, x9
	mov	w8, #1
loop1:
	add	x2, x10, x10, lsr #1	/* work out 3x current cache level */
	lsr	x1, x0, x2		/* extract cache type bits from clidr */
	and	x1, x1, #7		/* mask the bits for current cache only */
	cmp	x1, #2			/* see what cache we have at this level */
	b.lo	level_done		/* nothing to do if no cache or icache */

	msr	csselr_el1, x10		/* select current cache level in csselr */
	isb				/* sync the new csselr and ccsidr */
	mrs	x1, ccsidr_el1		/* read the new ccsidr */
	and	x2, x1, #7		/* extract the length of the cache lines */
	add	x2, x2, #4		/* add 4 (line length offset) */
	ubfx	x4, x1, #3, #10		/* maximum way number */
	clz	w5, w4			/* bit position of way size increment */
	lsl	w9, w4, w5		/* w9 = aligned max way number */
	lsl	w16, w8, w5		/* w16 = way number loop decrement */
	orr	w9, w10, w9		/* w9 = combine way and cache number */
	ubfx	w6, w1, #13, #15	/* w6 = max set number */
	lsl	w17, w8, w2		/* w17 = set number loop decrement */
	dsb	sy			/* barrier before we start this level */
	br	x14			/* jump to DC operation specific loop */

.macro	dcsw_loop _op
#if ENABLE_BTI
    bti	j
#endif
loop2_\_op:
    lsl	w7, w6, w2		/* w7 = aligned max set number */

loop3_\_op:
    orr	w11, w9, w7		/* combine cache, way and set number */
    dc	\_op, x11
    subs	w7, w7, w17		/* decrement set number */
    b.hs	loop3_\_op

    subs	x9, x9, x16		/* decrement way number */
    b.hs	loop2_\_op

    b	level_done
.endm

level_done:
	add	x10, x10, #2		/* increment cache number */
	cmp	x3, x10
	b.hi    loop1
	msr	csselr_el1, xzr		/* select cache level 0 in csselr */
	dsb	sy			/* barrier to complete final cache operation */
	isb
exit:
	ret
ENDPROC(do_dcsw_op)

dcsw_loop_table:
	dcsw_loop isw
	dcsw_loop cisw
	dcsw_loop csw

ENTRY(dcsw_op_louis)
    dcsw_op #LOUIS_SHIFT, #CLIDR_FIELD_WIDTH, #LEVEL_SHIFT
ENDPROC(dcsw_op_louis)

ENTRY(dcsw_op_all)
    dcsw_op #LOC_SHIFT, #CLIDR_FIELD_WIDTH, #LEVEL_SHIFT
ENDPROC(dcsw_op_all)

/*
 * 为指定层级的数据 Cache set/way 操作准备参数。
 */
.macro dcsw_op_level level
    mrs	x9, clidr_el1
    mov	x3, \level
    sub	x10, x3, #2
    b	do_dcsw_op
.endm

/*
 * 对 1 级 Cache 执行按 set/way 的数据 Cache 操作。
 * do_dcsw_op 需要 x0 为 arch.h 中定义的操作类型（0-2）。
 */
ENTRY(dcsw_op_level1)
    dcsw_op_level #(1 << LEVEL_SHIFT)
ENDPROC(dcsw_op_level1)

/*
 * 对 2 级 Cache 执行按 set/way 的数据 Cache 操作。
 * do_dcsw_op 需要 x0 为 arch.h 中定义的操作类型（0-2）。
 */
ENTRY(dcsw_op_level2)
    dcsw_op_level #(2 << LEVEL_SHIFT)
ENDPROC(dcsw_op_level2)

/*
 * 对 3 级 Cache 执行按 set/way 的数据 Cache 操作。
 * do_dcsw_op 需要 x0 为 arch.h 中定义的操作类型（0-2）。
 */
ENTRY(dcsw_op_level3)
    dcsw_op_level #(3 << LEVEL_SHIFT)
ENDPROC(dcsw_op_level3)
