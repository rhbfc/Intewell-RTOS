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
 * @file： crt1.S
 * @brief：
 *      <li>AArch64 启动与早期初始化代码。</li>
 */
/************************头 文 件******************************/
#define ASM_USE
#include <asm.h>
#include <cpu.h>

#include <el2_common_macros.S>

/************************宏 定 义******************************/

#define STACK_SIZE  0x1000

/************************类型定义******************************/

/************************外部声明******************************/

/************************前向声明******************************/

/* 按缓存层级执行 clean/invalidate，用于早期启动阶段的 Cache 维护。 */
.align 3
InvalidateFlushDcacheLevel:
    lsl    x12, x0, #1
    msr    csselr_el1, x12       /* select cache level */
    isb                          /* sync change of cssidr_el1 */
    mrs    x6, ccsidr_el1        /* read the new cssidr_el1 */
    and    x2, x6, #7            /* x2 <- log2(cache line size)-4 */
    add    x2, x2, #4            /* x2 <- log2(cache line size) */
    mov    x3, #0x3ff
    and    x3, x3, x6, lsr #3    /* x3 <- max number of #ways, ccsidr_el1[12:3] */
    clz    w5, w3                /* bit position of #ways */
    mov    x4, #0x7fff
    and    x4, x4, x6, lsr #13    /* x4 <- max number of #sets, ccsidr_el1[27:13] */
    /* x12 <- cache level << 1 */
    /* x2 <- line length offset */
    /* x3 <- number of cache ways - 1 */
    /* x4 <- number of cache sets - 1 */
    /* x5 <- bit position of #ways */

InvalidateFlushCacheSet:
    mov    x6, x3            /* x6 <- working copy of #ways */

InvalidateFlushCacheWay:
    lsl    x7, x6, x5		  /* x7 = x6 << x5 */
    orr    x9, x12, x7        /* x9 = x12 | x7,  map way and level to cisw value */
    lsl    x7, x4, x2
    orr    x9, x9, x7        /* map set number to cisw value */
    tbz    w1, #0, 1f		 /* x1 = 0f, flush cache */
    dc    isw, x9			 /* invalidate dcache */
    b    2f
1:    dc    cisw, x9         /* clean & invalidate by set/way */
2:    subs    x6, x6, #1        /* decrement the way */
    b.ge    InvalidateFlushCacheWay
    subs    x4, x4, #1        /* decrement the set */
    b.ge    InvalidateFlushCacheSet

    ret

.global InvalidateFlushDcaches
InvalidateFlushDcaches:
    mov    x1, x0                /* x1 = 0 flush, x1 = 1 invalidate */
    dsb    sy                    /* barrier for full system */
    mrs    x10, clidr_el1        /* read clidr_el1 */
    lsr    x11, x10, #24
    and    x11, x11, #0x7        /* x11 <- loc bit[26:24], level of cache hierarchy */
    cbz    x11, InvalidateFlushDcacheEnd        /* if loc is 0, no cache, exit */
    mov    x15, lr           /* preserve LR */
    mov    x0, #0            /* start flush at cache level 0 */
    /* x0  <- cache level */
    /* x10 <- clidr_el1 */
    /* x11 <- loc */
    /* x15 <- return address */

InvalidateFlushCachesLoopLevel:
    lsl    x12, x0, #1         /* x12 = x0 * 2 */
    add    x12, x12, x0        /* x0 <- tripled cache level */
    lsr    x12, x10, x12       /* get x10, clidr_el1[ctype-n] to x12 */
    and    x12, x12, #7        /* x12 <- cache type */
    cmp    x12, #2             /* if not 000(no-cache), 001(i-cache only) */
    b.lt   InvalidateFlushCachesSkipLevel /* skip if no cache or icache */
    bl     InvalidateFlushDcacheLevel     /* x1 = 0 flush, 1 invalidate */

InvalidateFlushCachesSkipLevel:
    add    x0, x0, #1        /* increment cache level */
    cmp    x11, x0
    b.gt   InvalidateFlushCachesLoopLevel

    mov    x0, #0
    msr    csselr_el1, x0        /* restore csselr_el1 */
    dsb    sy
    isb
    mov    lr, x15

InvalidateFlushDcacheEnd:
	ret


/************************模块变量******************************/

/************************函数实现******************************/
.section ".image_header", "ax"
.global _arm64_header
_arm64_header:
	b _start
	.word 0
	/* Image load offset from start of RAM, little-endian */
	.quad   0
	/* Effective size of kernel image, little-endian */
	.quad   __image_size__ /* 0x60000 - ignored */
	/* Informative flags, little-endian */
	.quad   0xa
	.quad   0                               /* reserved */
	.quad   0                               /* reserved */
	.quad   0                               /* reserved */
	.byte   0x41                            /* Magic number, "ARM\x64" */
	.byte   0x52
	.byte   0x4d
	.byte   0x64
	.word   0                               /* reserved */

/* void _start(void *boot_param, uint32_t boot_param_size) */
.section ".__start", "ax"
/* void start(void) */
ENTRY(_start)

    mov x21, x0

    /* 屏蔽所有中断 */
    msr daifset, #DISABLE_ALL_EXCEPTIONS

    TLBI VMALLE1
    ic IALLU
    bl InvalidateFlushDcaches
    dsb sy
    isb

    /* 记录物理地址与虚拟地址的偏移存于r7 */
    adr    x0, _start
    ldr    x1, =_start
    sub    x7, x0, x1

    /* 计算出整个内核大小存于r8 */
    ldr    x2, =__end__
    sub    x8, x2, x1

    /* 设置调用cache_dcache_flush的size参数 */
    mov    x1, x8

    /* Clean and invalidate VMK空间cache到 PoC */
    bl     cache_dcache_flush

    mrs x0, CurrentEl
    lsr x0, x0, #2
    cmp x0, #1
    beq cpu_in_el1

cpu_in_el2:
    mrs     x0, cnthctl_el2
    orr     x0, x0, EL1PCTEN_BIT
    msr     cnthctl_el2, x0
    msr     cntvoff_el2, xzr

    el2_arch_init_common

    mov_imm     x0, SPSR_64(MODE_EL1, MODE_SP_ELX, DISABLE_ALL_EXCEPTIONS)
    msr     spsr_el2, x0
    adr     x2, cpu_in_el1
    msr     elr_el2, x2

    mrs     x0, hcr_el2
    orr     x0, x0, #HCR_RW_BIT
    orr     x0, x0, #(1 << 1)
    msr     hcr_el2, x0
    eret

cpu_in_el1:
    mov x1, #( SCTLR_A_BIT | SCTLR_SA_BIT) /* Enable align check and sp align check */
    mrs x0, sctlr_el1
    // orr  x0, x0, x1
    bic x0, x0, x1
    bic x0, x0, #SCTLR_DSSBS_BIT
    bic x0, x0, #SCTLR_C_BIT /* Disable D cache */
    bic x0, x0, #SCTLR_M_BIT /* Disable MMU */
    bic x0, x0, #SCTLR_I_BIT /* Disable I cache */

    msr sctlr_el1, x0
    isb

    /* Avoid trap from SIMD or float point instruction */
    mov_imm  x1, CPACR_EL1_FPEN(CPACR_EL1_FP_TRAP_NONE)         /* Don't trap any SIMD/FP instructions in both EL0 and EL1 */
    msr      cpacr_el1, x1

    msr     spsel, #MODE_SP_ELX

    ldr x1, =_start
    adr x0, _start
    sub x9, x0, x1 //PV_OFFSET



    /* clear bss */
    ldr     x1, =__bss_start__     /* get bss start address */
    ldr     x2, =__bss_end__
    sub     x2, x2, x1            /* get bss size          */
    add     x1, x1, x9

    and     x3, x2, #7           /* x3 is < 7 */
    ldr     x4, =~0x7
    and     x2, x2, x4            /* mask ~7 */

.L__clean_bss_loop:
    cbz     x2, .L__clean_bss_loop_1
    str     xzr, [x1], #8
    sub     x2, x2, #8
    b       .L__clean_bss_loop

.L__clean_bss_loop_1:
    cbz     x3, .L__jump_to_entry
    strb    wzr, [x1], #1
    sub     x3, x3, #1
    b       .L__clean_bss_loop_1

.L__jump_to_entry:
    /* 构建 mmu的 tcr寄存器 */
    adrp x0, init_stack
    add  x0, x0, #STACK_SIZE
    mov sp, x0

    mov x0, x21
    bl dtb_copy

    bl mmu_mair_tcr_init

    /* 构建 初始mmu表 */
    adr x0, _start
    ldr x1, =_start
    sub x0, x0, x1 //PV_OFFSET
    bl mmu_setup_early

    ldr x30, =after_mmu_enable  /* set LR to after_mmu_enable function, it's a v_addr */

    b start_enbale_mmu
after_mmu_enable:
    msr ttbr0_el1, xzr
    dsb sy

    /* 设置异常向量表 */
    ldr x0, =vector_table
    msr vbar_el1, x0

    /* 重新设置栈 */
    ldr    x0, =init_stack
    add    sp, x0, #STACK_SIZE           /* sp_el1 set to _start */

    /* 低32位 */
    and x0, x7, #0xFFFFFFFF
    lsr x1, x7, #32  // 将 x7 右移 32 位，高 32 位存入 x1
    ldr x2, =_start
    bl kernel_mmu_set_pvoffset

    /* 使能所有中断 */
    msr daifclr, #(DAIF_ABT_BIT | DAIF_DBG_BIT)
    b  start_kernel

ENDPROC(_start)

.section ".__start", "ax"
/* 从核启动入口 */
ENTRY(_start_ap)
#ifndef CONFIG_S5000C
	tlbi vmalle1
	dsb  nsh
#endif
    /* 屏蔽所有中断 */
    msr daifset, #DISABLE_ALL_EXCEPTIONS

    /* 保存cpu index + 1*/
    add    x9, x0, #1

    mrs x0, CurrentEl
    lsr x0, x0, #2
    cmp x0, #1
    beq _ap_cpu_in_el1

_ap_cpu_in_el2:
    mrs     x0, cnthctl_el2
    orr     x0, x0, EL1PCTEN_BIT
    msr     cnthctl_el2, x0
    msr     cntvoff_el2, xzr

    el2_arch_init_common

    mov_imm     x0, SPSR_64(MODE_EL1, MODE_SP_ELX, DISABLE_ALL_EXCEPTIONS)
    msr     spsr_el2, x0
    adr     x2, _ap_cpu_in_el1
    msr     elr_el2, x2

    mrs     x0, hcr_el2
    orr     x0, x0, #HCR_RW_BIT
    orr     x0, x0, #(1 << 1)
    msr     hcr_el2, x0
    eret

_ap_cpu_in_el1:

    /* 浮点初始化 */
    // bl      fp_init

    /* 开启用户态访问 定时器 */
    mov x0, #2
    msr cntkctl_el1, x0

    /* 关MMU | 关数据CACHE | 关指令CACHE | 使能对齐检查 */
    mov x1, #( SCTLR_A_BIT | SCTLR_SA_BIT) /* Enable align check and sp align check */
    mrs x0, sctlr_el1
    // orr  x0, x0, x1
    bic x0, x0, x1
    bic x0, x0, #SCTLR_DSSBS_BIT
    bic x0, x0, #SCTLR_C_BIT /* Disable D cache */
    bic x0, x0, #SCTLR_M_BIT /* Disable MMU */
    bic x0, x0, #SCTLR_I_BIT /* Disable I cache */

    msr sctlr_el1, x0
    isb

    /* Avoid trap from SIMD or float point instruction */
    mov_imm  x1, CPACR_EL1_FPEN(CPACR_EL1_FP_TRAP_NONE)         /* Don't trap any SIMD/FP instructions in both EL0 and EL1 */
    msr      cpacr_el1, x1

    msr     spsel, #MODE_SP_ELX

    /* 记录物理地址与虚拟地址的偏移存于x7 */
    adr    x0, _start_ap
    ldr    x1, =_start_ap
    sub    x20, x0, x1

    /* 设置栈 */
    mov    x0, #STACK_SIZE
    mul    x9, x9, x0
    ldr    x0, =init_stack
    add    x9, x9, x0

    /* 获取物理地址 */
    mov    x21, x9
    add    sp, x21, x20

    /* 构建 mmu的 ttbcr寄存器 */
    bl      mmu_mair_tcr_init

    /* pvoffset */
    mov x0, x20

    bl ap_mmu_setup_early

    ldr    x0, =_kernel_mmu_table0
    add    x0, x0, x20

    msr ttbr1_el1, x0
    dsb sy

    ldr x30, =__ap_after_mmu_enable  /* set LR to after_mmu_enable function, it's a v_addr */
#ifdef CONFIG_S5000C
    b start_enbale_mmu
#else
    b start_enbale_mmu_ap
#endif

__ap_after_mmu_enable:
    msr ttbr0_el1, xzr
    dsb sy

    /* 设置异常向量表 */
    ldr    x0, =vector_table
    msr vbar_el1, x0

    /* 设置栈 */
    mov    sp, x21

    /* 使能所有中断 */
    msr daifclr, #(DAIF_ABT_BIT | DAIF_DBG_BIT)
    b      start_ap

ENDPROC(_start_ap)


ENTRY(start_enbale_mmu)
    mrs x0, sctlr_el1
    orr x0, x0, #SCTLR_I_BIT   /* I */
    orr x0, x0, #SCTLR_C_BIT    /* C */
    orr x0, x0, #SCTLR_M_BIT    /* M */
    msr sctlr_el1, x0        /* enable MMU */

    /* 无效TLB */
    dsb sy
    isb sy
    ic ialluis               /* Invalidate all instruction caches in Inner Shareable domain to Point of Unification */
    dsb sy
    isb sy
    tlbi vmalle1             /* Invalidate all stage 1 translations used at EL1 with the current VMID */
    dsb sy
    isb sy

    /* 获取物理地址 */
    adr    x7, _start

    ret
ENDPROC(start_enbale_mmu)

ENTRY(start_enbale_mmu_ap)
    mrs x0, sctlr_el1
    orr x0, x0, #SCTLR_I_BIT   /* I */
    orr x0, x0, #SCTLR_C_BIT    /* C */
    orr x0, x0, #SCTLR_M_BIT    /* M */
    msr sctlr_el1, x0        /* enable MMU */

    /* 获取物理地址 */
    adr    x7, _start

    ret
ENDPROC(start_enbale_mmu_ap)



.section ".data"
.align 3
init_stack:
    .space STACK_SIZE * CONFIG_MAX_CPUS
