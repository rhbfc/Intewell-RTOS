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
 * @file vclock_gettime.c
 * @brief AArch64 vDSO clock_gettime 实现辅助代码。
 */

#include <vdso_datapage.h>

extern const struct ttos_vdso_data __ttos_vdso_data;

static inline unsigned long long vdso_arch_read_cycles(void)
{
    unsigned long long val;

    /*
     * Kernel timer setup enables EL0 access for CNTVCT, not CNTPCT.
     * Use CNTVCT here to avoid EL0 system register trap in vDSO path.
     */
    __asm__ __volatile__("isb\n\tmrs %0, cntpct_el0" : "=r"(val));
    return val;
}

static inline const struct ttos_vdso_data *vdso_arch_data_ptr(void)
{
    const struct ttos_vdso_data *data;

    __asm__ __volatile__("adrp %0, __ttos_vdso_data\n\t"
                         "add %0, %0, :lo12:__ttos_vdso_data"
                         : "=r"(data));
    return data;
}

#include <vdso/vclock_gettime_common.h>

int __kernel_clock_gettime(clockid_t, struct __kernel_timespec *)
    __attribute__((weak, alias("__vdso_clock_gettime")));
