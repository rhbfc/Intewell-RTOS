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

#include <arch_timer.h>
#include <arch_types.h>
#include <clock/clockchip.h>
#include <cpuid.h>
#include <errno.h>
#include <limits.h>
#include <spinlock.h>
#include <stdio.h>
#include <time/ktime.h>
#include <ttos_pic.h>

#define KLOG_TAG "clockchip.c"
#include <klog.h>

int arch_timer_clockchip_init(void);

/** Control structure for clockchip manager */
struct clockchip_ctrl
{
    ttos_spinlock_t lock;
    struct list_head clkchip_list;
};

static struct clockchip_ctrl ccctrl;

static void default_event_handler(struct clockchip *cc)
{
    /* Just ignore. Do nothing. */
}

/**
 * @brief 设置时钟芯片的事件处理函数
 *
 * 该函数设置时钟芯片的事件处理函数
 *
 * @param[in] cc clockchip结构体指针
 * @param[in] event_handler 事件处理函数
 */
void clockchip_set_event_handler(struct clockchip *cc, void (*event_handler)(struct clockchip *))
{
    if (cc && event_handler)
    {
        cc->event_handler = event_handler;
    }
}

uint64_t ns_to_timer_count (uint64_t ns, uint64_t freq)
{
    return (1.0 * freq / NSEC_PER_SEC) * ns;
}

uint64_t timer_count_to_ns (uint64_t count, uint64_t freq)
{
    return count*(1.0*1000000000ULL/freq);
}

/**
 * @brief 时钟芯片初始化
 *
 * 该函数初始化时钟芯片
 *
 * @return 成功时返回0
 * @retval 0 成功
 */
int clockchip_init(void)
{
    arch_timer_clockchip_init();

    return 0;
}
