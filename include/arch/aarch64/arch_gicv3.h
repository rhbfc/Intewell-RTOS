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
 * @file： arch_gicv3.h
 * @brief：
 *	    <li>arch gicv3相关宏定义类型定义。</li>
 */

#ifndef _ARCH_GICV3_H
#define _ARCH_GICV3_H
/************************头文件********************************/
#include <cpu.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/************************宏定义********************************/

/************************类型定义******************************/

/**
 * @brief
 *    读寄存器ICC_SRE
 * @param[in] 无
 * @retval 值
 */
static inline u32 arch_gicv3_sre_read (void)
{
    return read_icc_sre_el1();
}

/**
 * @brief
 *    写寄存器ICC_SRE
 * @param[in] val 值
 * @retval 无
 */
static inline void arch_gicv3_sre_write (uintptr_t val)
{
    write_icc_sre_el1(val);
}

/**
 * @brief
 *    读寄存器MPIDR
 * @param[in] 无
 * @retval 值
 */
static inline u32 arch_gicv3_mpidr_read (void)
{
    return read_mpidr_el1();
}

/**
 * @brief
 *    写寄存器ICC_BPR1
 * @param[in] val 值
 * @retval 无
 */
static inline void arch_gicv3_bpr1_write (u32 val)
{
    // sysreg_write (val, ICC_BPR1);
}

/**
 * @brief
 *    写寄存器ICC_PMR
 * @param[in] val 值
 * @retval 无
 */
static inline void arch_gicv3_pmr_write (u32 val)
{
    write_icc_pmr_el1(val);
}

/**
 * @brief
 *    写寄存器ICC_IGRPEN1
 * @param[in] val 值
 * @retval 无
 */
static inline void arch_gicv3_igrpen1_write (u32 val)
{
    write_icc_igrpen1_el1(val);
}

/**
 * @brief
 *    读寄存器ICC_IAR1
 * @param[in] 无
 * @retval 值
 */
static inline u32 arch_gicv3_iar1_read (void)
{
    return  read_icc_iar1_el1();
}

/**
 * @brief
 *    写寄存器ICC_EOIR1
 * @param[in] val 值
 * @retval 无
 */
static inline void arch_gicv3_eoir1_write (u32 val)
{
    write_icc_eoir1_el1(val);
}

/**
 * @brief
 *    写寄存器ICC_SGI1R
 * @param[in] val 值
 * @retval 无
 */
static inline void arch_gicv3_sgi1r_write (u64 val)
{
    write_icc_sgi1r(val);
}

/************************外部声明******************************/

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* _ARCH_GICV3_H */
