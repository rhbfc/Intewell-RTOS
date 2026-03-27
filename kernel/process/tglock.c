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

#include <ttosProcess.h>
#include <ttos.h>
#include <assert.h>
#undef KLOG_TAG
#define KLOG_TAG "tglock"
#include <klog.h>

void tg_lock_create (pcb_t tg_leader)
{
    spin_lock_init(&tg_leader->tglock);
}

void tg_lock (pcb_t tg_leader, irq_flags_t *irq_flag)
{
    irq_flags_t flag;
    spin_lock_irqsave(&tg_leader->tglock, flag);
    *irq_flag = flag;
}

void tg_unlock (pcb_t tg_leader, irq_flags_t *irq_flag)
{
    irq_flags_t flag = *irq_flag;
    spin_unlock_irqrestore(&tg_leader->tglock, flag);
}

void tg_lock_delete (pcb_t pcb)
{

}

bool tg_lock_is_held (pcb_t tg_leader)
{
    return spin_is_locked(&tg_leader->tglock);
}

