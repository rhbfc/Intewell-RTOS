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

#ifndef _TG_LOCK_H
#define _TG_LOCK_H
#include <ttosProcess.h>

void tg_lock_create (pcb_t pcb);
void tg_lock (pcb_t pcb, irq_flags_t *irq_flag);
void tg_unlock (pcb_t pcb, irq_flags_t *irq_flag);
void tg_lock_delete (pcb_t pcb);
bool tg_lock_is_held (pcb_t tg_leader);

#endif