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

#ifndef _CLOCKCHIP_H__
#define _CLOCKCHIP_H__

#include <list.h>
#include <system/types.h>

/* Clockchip mode commands */
enum clockchip_mode
{
    CLOCKCHIP_MODE_UNUSED = 0,
    CLOCKCHIP_MODE_SHUTDOWN,
    CLOCKCHIP_MODE_PERIODIC,
    CLOCKCHIP_MODE_ONESHOT,
    CLOCKCHIP_MODE_RESUME,
};

struct clockchip;

struct clockchip {

    const char           *name;
    u32                   hw_irq;
    u32                   freq;
    void (*event_handler) (struct clockchip *cc);
    void (*set_mode) (enum clockchip_mode mode, struct clockchip *cc);
    int (*set_next_event) (unsigned long long evt, struct clockchip *cc);
    u32                 bound_on;
    u64                 next_event;
    void               *priv;
};

int clockchip_init (void);

#endif
