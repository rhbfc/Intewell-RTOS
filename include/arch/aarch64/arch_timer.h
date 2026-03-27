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
 * @file： arch_timer.h
 * @brief：
 *	    <li>arch timer相关宏定义类型定义。</li>
 */

#ifndef _ARCH_TIMER_H
#define _ARCH_TIMER_H
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
 *    写寄存器CNTP_TVAL
 * @param[in] val 值
 * @retval 无
 */
static inline void arch_timer_cntp_tval_write (u32 val)
{
    write_cntp_tval(val);
}

/**
 * @brief
 *    写寄存器CNTP_CVAL
 * @param[in] val 值
 * @retval 无
 */
static inline void arch_timer_cntp_cval_write (u64 val)
{
    write_cntp_cval(val);
}

/**
 * @brief
 *    读寄存器CNTP_CTL
 * @param[in] 无
 * @retval 值
 */
static inline u32 arch_timer_cntp_ctl_read (void)
{
    return read_cntp_ctl();
}

/**
 * @brief
 *    写寄存器CNTP_CTL
 * @param[in] val 值
 * @retval 无
 */
static inline void arch_timer_cntp_ctl_write (u32 val)
{
    write_cntp_ctl(val);
}

/**
 * @brief
 *    读寄存器CNTKCTL
 * @param[in] 无
 * @retval 值
 */
static inline u32 arch_timer_cntkctl_read (void)
{
    return read_cntkctl();
}

/**
 * @brief
 *    写寄存器CNTKCTL
 * @param[in] val 值
 * @retval 无
 */
static inline void arch_timer_cntkctl_write (u32 val)
{
    write_cntkctl(val);
}

/**
 * @brief
 *    读寄存器CNTFRQ
 * @param[in] 无
 * @retval 值
 */
static inline u32 arch_timer_cntfrq_read (void)
{
    return read_cntfrq();
}

/**
 * @brief
 *    读寄存器CNTPCT
 * @param[in] 无
 * @retval 值
 */
static inline u64 arch_timer_cntpct_read (void)
{
    return read_cntpct_el0();
}

/************************外部声明******************************/

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* _ARCH_TIMER_H */
