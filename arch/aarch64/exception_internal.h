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

#ifndef AARCH64_EXCEPTION_INTERNAL_H
#define AARCH64_EXCEPTION_INTERNAL_H

#include <context.h>
#include <stdbool.h>
#include <ttos.h>
#include <ttosProcess.h>

void *exception_sp_get(struct arch_context *context);
void set_context_type(struct arch_context *context, int type);

bool aarch64_exception_from_user(const struct arch_context *context);
bool aarch64_exception_from_kernel(const struct arch_context *context);
bool aarch64_exception_user_context_valid(struct arch_context *context);
pcb_t aarch64_exception_save_user_entry_context(struct arch_context *context, TASK_ID task);

#endif /* AARCH64_EXCEPTION_INTERNAL_H */
