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

#ifndef __AARCH64_ESR_H__
#define __AARCH64_ESR_H__

#include <cpu.h>
#include <stdbool.h>

/*
 * AArch64 ESR exception class values.
 * 数值来自架构定义，这里统一整理为项目内自有枚举与辅助宏。
 */
enum
{
    ESR_ELx_EC_UNKNOWN = 0x00,
    ESR_ELx_EC_WFx = 0x01,
    ESR_ELx_EC_CP15_32 = 0x03,
    ESR_ELx_EC_CP15_64 = 0x04,
    ESR_ELx_EC_CP14_MR = 0x05,
    ESR_ELx_EC_CP14_LS = 0x06,
    ESR_ELx_EC_FP_ASIMD = 0x07,
    ESR_ELx_EC_CP10_ID = 0x08,
    ESR_ELx_EC_PAC = 0x09,
    ESR_ELx_EC_CP14_64 = 0x0C,
    ESR_ELx_EC_BTI = 0x0D,
    ESR_ELx_EC_ILL = 0x0E,
    ESR_ELx_EC_SVC32 = 0x11,
    ESR_ELx_EC_HVC32 = 0x12,
    ESR_ELx_EC_SMC32 = 0x13,
    ESR_ELx_EC_SVC64 = 0x15,
    ESR_ELx_EC_HVC64 = 0x16,
    ESR_ELx_EC_SMC64 = 0x17,
    ESR_ELx_EC_SYS64 = 0x18,
    ESR_ELx_EC_SVE = 0x19,
    ESR_ELx_EC_ERET = 0x1A,
    ESR_ELx_EC_FPAC = 0x1C,
    ESR_ELx_EC_SME = 0x1D,
    ESR_ELx_EC_IMP_DEF = 0x1F,
    ESR_ELx_EC_IABT_LOW = 0x20,
    ESR_ELx_EC_IABT_CUR = 0x21,
    ESR_ELx_EC_PC_ALIGN = 0x22,
    ESR_ELx_EC_DABT_LOW = 0x24,
    ESR_ELx_EC_DABT_CUR = 0x25,
    ESR_ELx_EC_SP_ALIGN = 0x26,
    ESR_ELx_EC_MOPS = 0x27,
    ESR_ELx_EC_FP_EXC32 = 0x28,
    ESR_ELx_EC_FP_EXC64 = 0x2C,
    ESR_ELx_EC_SERROR = 0x2F,
    ESR_ELx_EC_BREAKPT_LOW = 0x30,
    ESR_ELx_EC_BREAKPT_CUR = 0x31,
    ESR_ELx_EC_SOFTSTP_LOW = 0x32,
    ESR_ELx_EC_SOFTSTP_CUR = 0x33,
    ESR_ELx_EC_WATCHPT_LOW = 0x34,
    ESR_ELx_EC_WATCHPT_CUR = 0x35,
    ESR_ELx_EC_BKPT32 = 0x38,
    ESR_ELx_EC_VECTOR32 = 0x3A,
    ESR_ELx_EC_BRK64 = 0x3C,
    ESR_ELx_EC_MAX = 0x3F,
};

#define ESR_ELx_EC_SHIFT (26)
#define ESR_ELx_EC_WIDTH (6)
#define ESR_ELx_EC_MASK (UL(0x3F) << ESR_ELx_EC_SHIFT)
#define ESR_ELx_EC(esr) (((esr)&ESR_ELx_EC_MASK) >> ESR_ELx_EC_SHIFT)

#define ESR_ELx_IL_SHIFT (25)
#define ESR_ELx_IL (UL(1) << ESR_ELx_IL_SHIFT)
#define ESR_ELx_ISS_MASK (GENMASK(24, 0))
#define ESR_ELx_ISS(esr) ((esr)&ESR_ELx_ISS_MASK)
#define ESR_ELx_ISS2_SHIFT (32)
#define ESR_ELx_ISS2_MASK (GENMASK_ULL(55, 32))
#define ESR_ELx_ISS2(esr) (((esr)&ESR_ELx_ISS2_MASK) >> ESR_ELx_ISS2_SHIFT)

/* 各类异常共享的 ISS 位。 */
#define ESR_ELx_WNR_SHIFT (6)
#define ESR_ELx_WNR (UL(1) << ESR_ELx_WNR_SHIFT)

/* 异步错误类型。 */
#define ESR_ELx_IDS_SHIFT (24)
#define ESR_ELx_IDS (UL(1) << ESR_ELx_IDS_SHIFT)
#define ESR_ELx_AET_SHIFT (10)
#define ESR_ELx_AET (UL(0x7) << ESR_ELx_AET_SHIFT)

#define ESR_ELx_AET_UC (UL(0) << ESR_ELx_AET_SHIFT)
#define ESR_ELx_AET_UEU (UL(1) << ESR_ELx_AET_SHIFT)
#define ESR_ELx_AET_UEO (UL(2) << ESR_ELx_AET_SHIFT)
#define ESR_ELx_AET_UER (UL(3) << ESR_ELx_AET_SHIFT)
#define ESR_ELx_AET_CE (UL(6) << ESR_ELx_AET_SHIFT)

/* 指令/数据访存异常共享位。 */
#define ESR_ELx_SET_SHIFT (11)
#define ESR_ELx_SET_MASK (UL(3) << ESR_ELx_SET_SHIFT)
#define ESR_ELx_FnV_SHIFT (10)
#define ESR_ELx_FnV (UL(1) << ESR_ELx_FnV_SHIFT)
#define ESR_ELx_EA_SHIFT (9)
#define ESR_ELx_EA (UL(1) << ESR_ELx_EA_SHIFT)
#define ESR_ELx_S1PTW_SHIFT (7)
#define ESR_ELx_S1PTW (UL(1) << ESR_ELx_S1PTW_SHIFT)

/* 指令/数据访存异常共用的 FSC 编码。 */
#define ESR_ELx_FSC (0x3F)
#define ESR_ELx_FSC_TYPE (0x3C)
#define ESR_ELx_FSC_LEVEL (0x03)
#define ESR_ELx_FSC_EXTABT (0x10)
#define ESR_ELx_FSC_MTE (0x11)
#define ESR_ELx_FSC_SERROR (0x11)
#define ESR_ELx_FSC_ACCESS (0x08)
#define ESR_ELx_FSC_FAULT (0x04)
#define ESR_ELx_FSC_PERM (0x0C)
#define ESR_ELx_FSC_SEA_TTW(n) (0x14 + (n))
#define ESR_ELx_FSC_SECC (0x18)
#define ESR_ELx_FSC_SECC_TTW(n) (0x1c + (n))

/* 分级页表对应的访问类型编码。 */
#define ESR_ELx_FSC_ACCESS_L(n) (ESR_ELx_FSC_ACCESS + n)
#define ESR_ELx_FSC_PERM_L(n) (ESR_ELx_FSC_PERM + n)

#define ESR_ELx_FSC_FAULT_nL (0x2C)
#define ESR_ELx_FSC_FAULT_L(n) (((n) < 0 ? ESR_ELx_FSC_FAULT_nL : ESR_ELx_FSC_FAULT) + (n))

/* Data Abort 相关 ISS 位。 */
#define ESR_ELx_ISV_SHIFT (24)
#define ESR_ELx_ISV (UL(1) << ESR_ELx_ISV_SHIFT)
#define ESR_ELx_SAS_SHIFT (22)
#define ESR_ELx_SAS (UL(3) << ESR_ELx_SAS_SHIFT)
#define ESR_ELx_SSE_SHIFT (21)
#define ESR_ELx_SSE (UL(1) << ESR_ELx_SSE_SHIFT)
#define ESR_ELx_SRT_SHIFT (16)
#define ESR_ELx_SRT_MASK (UL(0x1F) << ESR_ELx_SRT_SHIFT)
#define ESR_ELx_SF_SHIFT (15)
#define ESR_ELx_SF (UL(1) << ESR_ELx_SF_SHIFT)
#define ESR_ELx_AR_SHIFT (14)
#define ESR_ELx_AR (UL(1) << ESR_ELx_AR_SHIFT)
#define ESR_ELx_CM_SHIFT (8)
#define ESR_ELx_CM (UL(1) << ESR_ELx_CM_SHIFT)

/* Data Abort 相关 ISS2 位。 */
#define ESR_ELx_TnD_SHIFT (10)
#define ESR_ELx_TnD (UL(1) << ESR_ELx_TnD_SHIFT)
#define ESR_ELx_TagAccess_SHIFT (9)
#define ESR_ELx_TagAccess (UL(1) << ESR_ELx_TagAccess_SHIFT)
#define ESR_ELx_GCS_SHIFT (8)
#define ESR_ELx_GCS (UL(1) << ESR_ELx_GCS_SHIFT)
#define ESR_ELx_Overlay_SHIFT (6)
#define ESR_ELx_Overlay (UL(1) << ESR_ELx_Overlay_SHIFT)
#define ESR_ELx_DirtyBit_SHIFT (5)
#define ESR_ELx_DirtyBit (UL(1) << ESR_ELx_DirtyBit_SHIFT)
#define ESR_ELx_Xs_SHIFT (0)
#define ESR_ELx_Xs_MASK (GENMASK_ULL(4, 0))

/* 进入高异常级时使用的辅助字段。 */
#define ESR_ELx_FSC_ADDRSZ (0x00)
#define ESR_ELx_CV (UL(1) << 24)
#define ESR_ELx_COND_SHIFT (20)
#define ESR_ELx_COND_MASK (UL(0xF) << ESR_ELx_COND_SHIFT)
#define ESR_ELx_WFx_ISS_RN (UL(0x1F) << 5)
#define ESR_ELx_WFx_ISS_RV (UL(1) << 2)
#define ESR_ELx_WFx_ISS_TI (UL(3) << 0)
#define ESR_ELx_WFx_ISS_WFxT (UL(2) << 0)
#define ESR_ELx_WFx_ISS_WFI (UL(0) << 0)
#define ESR_ELx_WFx_ISS_WFE (UL(1) << 0)
#define ESR_ELx_xVC_IMM_MASK ((UL(1) << 16) - 1)

#define DISR_EL1_IDS (UL(1) << 24)
/*
 * DISR_EL1 and ESR_ELx share the bottom 13 bits, but the RES0 bits may mean
 * different things in the future...
 */
#define DISR_EL1_ESR_MASK (ESR_ELx_AET | ESR_ELx_EA | ESR_ELx_FSC)

/* 常见 ESR 组合值。 */
#define ESR_ELx_WFx_MASK (ESR_ELx_EC_MASK | (ESR_ELx_WFx_ISS_TI & ~ESR_ELx_WFx_ISS_WFxT))
#define ESR_ELx_WFx_WFI_VAL ((ESR_ELx_EC_WFx << ESR_ELx_EC_SHIFT) | ESR_ELx_WFx_ISS_WFI)

/* AArch64 BRK 指令附带 comment 字段。 */
#define ESR_ELx_BRK64_ISS_COMMENT_MASK 0xffff

/* System instruction trap 相关 ISS 位。 */
#define ESR_ELx_SYS64_ISS_RES0_SHIFT 22
#define ESR_ELx_SYS64_ISS_RES0_MASK (UL(0x7) << ESR_ELx_SYS64_ISS_RES0_SHIFT)
#define ESR_ELx_SYS64_ISS_DIR_MASK 0x1
#define ESR_ELx_SYS64_ISS_DIR_READ 0x1
#define ESR_ELx_SYS64_ISS_DIR_WRITE 0x0

#define ESR_ELx_SYS64_ISS_RT_SHIFT 5
#define ESR_ELx_SYS64_ISS_RT_MASK (UL(0x1f) << ESR_ELx_SYS64_ISS_RT_SHIFT)
#define ESR_ELx_SYS64_ISS_CRM_SHIFT 1
#define ESR_ELx_SYS64_ISS_CRM_MASK (UL(0xf) << ESR_ELx_SYS64_ISS_CRM_SHIFT)
#define ESR_ELx_SYS64_ISS_CRN_SHIFT 10
#define ESR_ELx_SYS64_ISS_CRN_MASK (UL(0xf) << ESR_ELx_SYS64_ISS_CRN_SHIFT)
#define ESR_ELx_SYS64_ISS_OP1_SHIFT 14
#define ESR_ELx_SYS64_ISS_OP1_MASK (UL(0x7) << ESR_ELx_SYS64_ISS_OP1_SHIFT)
#define ESR_ELx_SYS64_ISS_OP2_SHIFT 17
#define ESR_ELx_SYS64_ISS_OP2_MASK (UL(0x7) << ESR_ELx_SYS64_ISS_OP2_SHIFT)
#define ESR_ELx_SYS64_ISS_OP0_SHIFT 20
#define ESR_ELx_SYS64_ISS_OP0_MASK (UL(0x3) << ESR_ELx_SYS64_ISS_OP0_SHIFT)
#define ESR_ELx_SYS64_ISS_SYS_MASK                                                                 \
    (ESR_ELx_SYS64_ISS_OP0_MASK | ESR_ELx_SYS64_ISS_OP1_MASK | ESR_ELx_SYS64_ISS_OP2_MASK |        \
     ESR_ELx_SYS64_ISS_CRN_MASK | ESR_ELx_SYS64_ISS_CRM_MASK)
#define ESR_ELx_SYS64_ISS_SYS_VAL(op0, op1, op2, crn, crm)                                         \
    (((op0) << ESR_ELx_SYS64_ISS_OP0_SHIFT) | ((op1) << ESR_ELx_SYS64_ISS_OP1_SHIFT) |             \
     ((op2) << ESR_ELx_SYS64_ISS_OP2_SHIFT) | ((crn) << ESR_ELx_SYS64_ISS_CRN_SHIFT) |             \
     ((crm) << ESR_ELx_SYS64_ISS_CRM_SHIFT))

#define ESR_ELx_SYS64_ISS_SYS_OP_MASK (ESR_ELx_SYS64_ISS_SYS_MASK | ESR_ELx_SYS64_ISS_DIR_MASK)
#define ESR_ELx_SYS64_ISS_RT(esr) (((esr)&ESR_ELx_SYS64_ISS_RT_MASK) >> ESR_ELx_SYS64_ISS_RT_SHIFT)
/* EL0 cache op 的系统寄存器编码辅助值。 */
#define ESR_ELx_SYS64_ISS_CRM_DC_CIVAC 14
#define ESR_ELx_SYS64_ISS_CRM_DC_CVADP 13
#define ESR_ELx_SYS64_ISS_CRM_DC_CVAP 12
#define ESR_ELx_SYS64_ISS_CRM_DC_CVAU 11
#define ESR_ELx_SYS64_ISS_CRM_DC_CVAC 10
#define ESR_ELx_SYS64_ISS_CRM_IC_IVAU 5

#define ESR_ELx_SYS64_ISS_EL0_CACHE_OP_MASK                                                        \
    (ESR_ELx_SYS64_ISS_OP0_MASK | ESR_ELx_SYS64_ISS_OP1_MASK | ESR_ELx_SYS64_ISS_OP2_MASK |        \
     ESR_ELx_SYS64_ISS_CRN_MASK | ESR_ELx_SYS64_ISS_DIR_MASK)
#define ESR_ELx_SYS64_ISS_EL0_CACHE_OP_VAL                                                         \
    (ESR_ELx_SYS64_ISS_SYS_VAL(1, 3, 1, 7, 0) | ESR_ELx_SYS64_ISS_DIR_WRITE)
/* 可模拟的 EL0 MRS 指令编码辅助值。 */
#define ESR_ELx_SYS64_ISS_SYS_MRS_OP_MASK                                                          \
    (ESR_ELx_SYS64_ISS_OP0_MASK | ESR_ELx_SYS64_ISS_OP1_MASK | ESR_ELx_SYS64_ISS_CRN_MASK |        \
     ESR_ELx_SYS64_ISS_DIR_MASK)
#define ESR_ELx_SYS64_ISS_SYS_MRS_OP_VAL                                                           \
    (ESR_ELx_SYS64_ISS_SYS_VAL(3, 0, 0, 0, 0) | ESR_ELx_SYS64_ISS_DIR_READ)

#define ESR_ELx_SYS64_ISS_SYS_CTR ESR_ELx_SYS64_ISS_SYS_VAL(3, 3, 1, 0, 0)
#define ESR_ELx_SYS64_ISS_SYS_CTR_READ (ESR_ELx_SYS64_ISS_SYS_CTR | ESR_ELx_SYS64_ISS_DIR_READ)

#define ESR_ELx_SYS64_ISS_SYS_CNTVCT                                                               \
    (ESR_ELx_SYS64_ISS_SYS_VAL(3, 3, 2, 14, 0) | ESR_ELx_SYS64_ISS_DIR_READ)

#define ESR_ELx_SYS64_ISS_SYS_CNTVCTSS                                                             \
    (ESR_ELx_SYS64_ISS_SYS_VAL(3, 3, 6, 14, 0) | ESR_ELx_SYS64_ISS_DIR_READ)

#define ESR_ELx_SYS64_ISS_SYS_CNTFRQ                                                               \
    (ESR_ELx_SYS64_ISS_SYS_VAL(3, 3, 0, 14, 0) | ESR_ELx_SYS64_ISS_DIR_READ)

#define esr_sys64_to_sysreg(e)                                                                     \
    sys_reg((((e)&ESR_ELx_SYS64_ISS_OP0_MASK) >> ESR_ELx_SYS64_ISS_OP0_SHIFT),                     \
            (((e)&ESR_ELx_SYS64_ISS_OP1_MASK) >> ESR_ELx_SYS64_ISS_OP1_SHIFT),                     \
            (((e)&ESR_ELx_SYS64_ISS_CRN_MASK) >> ESR_ELx_SYS64_ISS_CRN_SHIFT),                     \
            (((e)&ESR_ELx_SYS64_ISS_CRM_MASK) >> ESR_ELx_SYS64_ISS_CRM_SHIFT),                     \
            (((e)&ESR_ELx_SYS64_ISS_OP2_MASK) >> ESR_ELx_SYS64_ISS_OP2_SHIFT))

#define esr_cp15_to_sysreg(e)                                                                      \
    sys_reg(3, (((e)&ESR_ELx_SYS64_ISS_OP1_MASK) >> ESR_ELx_SYS64_ISS_OP1_SHIFT),                  \
            (((e)&ESR_ELx_SYS64_ISS_CRN_MASK) >> ESR_ELx_SYS64_ISS_CRN_SHIFT),                     \
            (((e)&ESR_ELx_SYS64_ISS_CRM_MASK) >> ESR_ELx_SYS64_ISS_CRM_SHIFT),                     \
            (((e)&ESR_ELx_SYS64_ISS_OP2_MASK) >> ESR_ELx_SYS64_ISS_OP2_SHIFT))

/* ERET/ERETAA/ERETAB trap 辅助位。 */
#define ESR_ELx_ERET_ISS_ERET 0x2
#define ESR_ELx_ERET_ISS_ERETA 0x1

/* 浮点异常 trap 的专用位。 */
#define ESR_ELx_FP_EXC_TFV (UL(1) << 23)

/* CP15 相关 trap 编码。 */
#define ESR_ELx_CP15_32_ISS_DIR_MASK 0x1
#define ESR_ELx_CP15_32_ISS_DIR_READ 0x1
#define ESR_ELx_CP15_32_ISS_DIR_WRITE 0x0

#define ESR_ELx_CP15_32_ISS_RT_SHIFT 5
#define ESR_ELx_CP15_32_ISS_RT_MASK (UL(0x1f) << ESR_ELx_CP15_32_ISS_RT_SHIFT)
#define ESR_ELx_CP15_32_ISS_CRM_SHIFT 1
#define ESR_ELx_CP15_32_ISS_CRM_MASK (UL(0xf) << ESR_ELx_CP15_32_ISS_CRM_SHIFT)
#define ESR_ELx_CP15_32_ISS_CRN_SHIFT 10
#define ESR_ELx_CP15_32_ISS_CRN_MASK (UL(0xf) << ESR_ELx_CP15_32_ISS_CRN_SHIFT)
#define ESR_ELx_CP15_32_ISS_OP1_SHIFT 14
#define ESR_ELx_CP15_32_ISS_OP1_MASK (UL(0x7) << ESR_ELx_CP15_32_ISS_OP1_SHIFT)
#define ESR_ELx_CP15_32_ISS_OP2_SHIFT 17
#define ESR_ELx_CP15_32_ISS_OP2_MASK (UL(0x7) << ESR_ELx_CP15_32_ISS_OP2_SHIFT)

#define ESR_ELx_CP15_32_ISS_SYS_MASK                                                               \
    (ESR_ELx_CP15_32_ISS_OP1_MASK | ESR_ELx_CP15_32_ISS_OP2_MASK | ESR_ELx_CP15_32_ISS_CRN_MASK |  \
     ESR_ELx_CP15_32_ISS_CRM_MASK | ESR_ELx_CP15_32_ISS_DIR_MASK)
#define ESR_ELx_CP15_32_ISS_SYS_VAL(op1, op2, crn, crm)                                            \
    (((op1) << ESR_ELx_CP15_32_ISS_OP1_SHIFT) | ((op2) << ESR_ELx_CP15_32_ISS_OP2_SHIFT) |         \
     ((crn) << ESR_ELx_CP15_32_ISS_CRN_SHIFT) | ((crm) << ESR_ELx_CP15_32_ISS_CRM_SHIFT))

#define ESR_ELx_CP15_64_ISS_DIR_MASK 0x1
#define ESR_ELx_CP15_64_ISS_DIR_READ 0x1
#define ESR_ELx_CP15_64_ISS_DIR_WRITE 0x0

#define ESR_ELx_CP15_64_ISS_RT_SHIFT 5
#define ESR_ELx_CP15_64_ISS_RT_MASK (UL(0x1f) << ESR_ELx_CP15_64_ISS_RT_SHIFT)

#define ESR_ELx_CP15_64_ISS_RT2_SHIFT 10
#define ESR_ELx_CP15_64_ISS_RT2_MASK (UL(0x1f) << ESR_ELx_CP15_64_ISS_RT2_SHIFT)

#define ESR_ELx_CP15_64_ISS_OP1_SHIFT 16
#define ESR_ELx_CP15_64_ISS_OP1_MASK (UL(0xf) << ESR_ELx_CP15_64_ISS_OP1_SHIFT)
#define ESR_ELx_CP15_64_ISS_CRM_SHIFT 1
#define ESR_ELx_CP15_64_ISS_CRM_MASK (UL(0xf) << ESR_ELx_CP15_64_ISS_CRM_SHIFT)

#define ESR_ELx_CP15_64_ISS_SYS_VAL(op1, crm)                                                      \
    (((op1) << ESR_ELx_CP15_64_ISS_OP1_SHIFT) | ((crm) << ESR_ELx_CP15_64_ISS_CRM_SHIFT))

#define ESR_ELx_CP15_64_ISS_SYS_MASK                                                               \
    (ESR_ELx_CP15_64_ISS_OP1_MASK | ESR_ELx_CP15_64_ISS_CRM_MASK | ESR_ELx_CP15_64_ISS_DIR_MASK)

#define ESR_ELx_CP15_64_ISS_SYS_CNTVCT                                                             \
    (ESR_ELx_CP15_64_ISS_SYS_VAL(1, 14) | ESR_ELx_CP15_64_ISS_DIR_READ)

#define ESR_ELx_CP15_64_ISS_SYS_CNTVCTSS                                                           \
    (ESR_ELx_CP15_64_ISS_SYS_VAL(9, 14) | ESR_ELx_CP15_64_ISS_DIR_READ)

#define ESR_ELx_CP15_32_ISS_SYS_CNTFRQ                                                             \
    (ESR_ELx_CP15_32_ISS_SYS_VAL(0, 0, 14, 0) | ESR_ELx_CP15_32_ISS_DIR_READ)

/* SME trap 子类型。 */

#define ESR_ELx_SME_ISS_SME_DISABLED 0
#define ESR_ELx_SME_ISS_ILL 1
#define ESR_ELx_SME_ISS_SM_DISABLED 2
#define ESR_ELx_SME_ISS_ZA_DISABLED 3
#define ESR_ELx_SME_ISS_ZT_DISABLED 4

/* MOPS trap 编码辅助。 */
#define ESR_ELx_MOPS_ISS_MEM_INST (UL(1) << 24)
#define ESR_ELx_MOPS_ISS_FROM_EPILOGUE (UL(1) << 18)
#define ESR_ELx_MOPS_ISS_WRONG_OPTION (UL(1) << 17)
#define ESR_ELx_MOPS_ISS_OPTION_A (UL(1) << 16)
#define ESR_ELx_MOPS_ISS_DESTREG(esr) (((esr) & (UL(0x1f) << 10)) >> 10)
#define ESR_ELx_MOPS_ISS_SRCREG(esr) (((esr) & (UL(0x1f) << 5)) >> 5)
#define ESR_ELx_MOPS_ISS_SIZEREG(esr) (((esr) & (UL(0x1f) << 0)) >> 0)

#ifndef __ASSEMBLY__

static inline unsigned long esr_brk_comment(unsigned long esr)
{
    return esr & ESR_ELx_BRK64_ISS_COMMENT_MASK;
}

static inline bool esr_is_data_abort(unsigned long esr)
{
    const unsigned long ec = ESR_ELx_EC(esr);

    return ec == ESR_ELx_EC_DABT_LOW || ec == ESR_ELx_EC_DABT_CUR;
}

static inline bool esr_fsc_is_translation_fault(unsigned long esr)
{
    esr = esr & ESR_ELx_FSC;

    return (esr == ESR_ELx_FSC_FAULT_L(3)) || (esr == ESR_ELx_FSC_FAULT_L(2)) ||
           (esr == ESR_ELx_FSC_FAULT_L(1)) || (esr == ESR_ELx_FSC_FAULT_L(0)) ||
           (esr == ESR_ELx_FSC_FAULT_L(-1));
}

static inline bool esr_fsc_is_permission_fault(unsigned long esr)
{
    esr = esr & ESR_ELx_FSC;

    return (esr == ESR_ELx_FSC_PERM_L(3)) || (esr == ESR_ELx_FSC_PERM_L(2)) ||
           (esr == ESR_ELx_FSC_PERM_L(1)) || (esr == ESR_ELx_FSC_PERM_L(0));
}

static inline bool esr_fsc_is_access_flag_fault(unsigned long esr)
{
    esr = esr & ESR_ELx_FSC;

    return (esr == ESR_ELx_FSC_ACCESS_L(3)) || (esr == ESR_ELx_FSC_ACCESS_L(2)) ||
           (esr == ESR_ELx_FSC_ACCESS_L(1)) || (esr == ESR_ELx_FSC_ACCESS_L(0));
}

/* 判断 ESR.EC==ERET 时是否为 ERETAx。 */
static inline bool esr_iss_is_eretax(unsigned long esr)
{
    return esr & ESR_ELx_ERET_ISS_ERET;
}

/* 判断 ERETAx 使用 A-Key 还是 B-Key。 */
static inline bool esr_iss_is_eretab(unsigned long esr)
{
    return esr & ESR_ELx_ERET_ISS_ERETA;
}

const char *esr_get_class_string(unsigned long esr);
#endif /* __ASSEMBLY */

#endif /* __AARCH64_ESR_H__ */
