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

#include <fault_recovery_table.h>

#if defined(CONFIG_SUPPORT_FAULT_RECOVERY) && (defined(__aarch64__) || defined(__x86_64__) || defined(__arm__))

extern fault_recovery_record_t __start_fault_recovery[];
extern fault_recovery_record_t __stop_fault_recovery[];

/*
 * Built-in kernel __fault_recovery can use binary search because
 * build-env/tools/sort_fault_recovery.py rewrites the final kernel ELF image
 * after link. The tool temporarily expands each entry into absolute
 * fault/continuation addresses, sorts by the faulting program counter, and
 * then re-encodes the relative form back into the image. This guarantees
 * [__start_fault_recovery, __stop_fault_recovery) stays ordered by
 * fault_recovery_record_fault_pc().
 */
const fault_recovery_record_t *find_fault_recovery_record(unsigned long addr)
{
    const fault_recovery_record_t *first = __start_fault_recovery;
    const fault_recovery_record_t *last = __stop_fault_recovery;

    while (first < last)
    {
        const fault_recovery_record_t *mid = first + ((last - first) >> 1);
        unsigned long mid_addr = fault_recovery_record_fault_pc(mid);

        if (mid_addr < addr)
        {
            first = mid + 1;
            continue;
        }

        if (mid_addr > addr)
        {
            last = mid;
            continue;
        }

        return mid;
    }

    return NULL;
}

#else

const fault_recovery_record_t *find_fault_recovery_record(unsigned long addr)
{
    (void)addr;

    return NULL;
}

bool try_apply_fault_recovery(arch_exception_context_t *context)
{
    (void)context;

    return false;
}

#endif /* CONFIG_SUPPORT_FAULT_RECOVERY && (__aarch64__ || __x86_64__ || __arm__) */
