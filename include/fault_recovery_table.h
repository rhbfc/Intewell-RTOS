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

#ifndef __FAULT_RECOVERY_TABLE_H__
#define __FAULT_RECOVERY_TABLE_H__

#include <context.h>
#include <fault_recovery_encoding.h>
#include <stdbool.h>
#include <system/compiler.h>
#include <system/types.h>

typedef struct fault_recovery_record
{
    s32 fault_pc_rel;
    s32 resume_pc_rel;
    u16 action_code;
    u16 action_data;
} __packed fault_recovery_record_t;

/*
 * The global fault-recovery table is only populated by built-in kernel
 * uaccess helpers. Loadable modules do not contribute entries today.
 */

static inline unsigned long fault_recovery_record_fault_pc(const fault_recovery_record_t *record)
{
    return (unsigned long)&record->fault_pc_rel + record->fault_pc_rel;
}

static inline unsigned long fault_recovery_record_resume_pc(const fault_recovery_record_t *record)
{
    return (unsigned long)&record->resume_pc_rel + record->resume_pc_rel;
}

static inline unsigned int fault_recovery_record_meta_field(u16 packed, u16 mask, unsigned int shift)
{
    return (packed & mask) >> shift;
}

static inline unsigned int fault_recovery_record_status_slot(const fault_recovery_record_t *record)
{
    return fault_recovery_record_meta_field(record->action_data,
                                            FAULT_RECOVERY_STATUS_MASK,
                                            FAULT_RECOVERY_STATUS_SHIFT);
}

static inline unsigned int fault_recovery_record_clear_slot(const fault_recovery_record_t *record)
{
    return fault_recovery_record_meta_field(record->action_data,
                                            FAULT_RECOVERY_CLEAR_MASK,
                                            FAULT_RECOVERY_CLEAR_SHIFT);
}

const fault_recovery_record_t *find_fault_recovery_record(unsigned long addr);
bool try_apply_fault_recovery(arch_exception_context_t *context);

#endif /* __FAULT_RECOVERY_TABLE_H__ */
