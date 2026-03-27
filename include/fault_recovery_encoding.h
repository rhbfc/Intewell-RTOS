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

#ifndef __FAULT_RECOVERY_ENCODING_H__
#define __FAULT_RECOVERY_ENCODING_H__

/*
 * Built-in kernel uaccess fault-recovery entries carry two pieces of
 * information:
 * 1. what kind of continuation the exception path should take
 * 2. optional packed metadata needed by that continuation
 */
#define FAULT_RECOVERY_ACTION_RESUME           0x0001
#define FAULT_RECOVERY_ACTION_ERRNO_AND_CLEAR  0x0002

/*
 * Packed metadata layout shared by the current recovery actions.
 * The low five bits select the status register slot, and the next
 * five bits select an optional register slot to be cleared.
 */
#define FAULT_RECOVERY_STATUS_SHIFT   0
#define FAULT_RECOVERY_STATUS_MASK    0x001f
#define FAULT_RECOVERY_CLEAR_SHIFT    5
#define FAULT_RECOVERY_CLEAR_MASK     0x03e0
#define FAULT_RECOVERY_SLOT_NONE      31

/*
 * Reserved for copy-direction annotations if a future recovery path
 * needs to distinguish read-side and write-side faults.
 */
#define FAULT_RECOVERY_COPY_IS_STORE  0x0001

#endif /* __FAULT_RECOVERY_ENCODING_H__ */
