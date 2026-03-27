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
#include <clock/clocksource.h>
#include <spinlock.h>
#include <stdio.h>
#include <string.h>
#include <ttos.h>
#include <time/ktime.h>

int arch_timer_freq_init (void);

/** Control structure for clocksource manager */
struct clocksource_ctrl {
    ttos_spinlock_t  lock;
    struct list_head clksrc_list;
};

static struct clocksource_ctrl csctrl;

/**
 * @brief 注册一个时钟源
 *
 * 该函数注册一个时钟源
 *
 * @param[in] cs timer_clocksource结构体指针
 * @return 成功时返回0，失败时返回-1
 */
int clocksource_register (struct timer_clocksource *cs)
{
    bool                      found;
    irq_flags_t               flags;
    struct timer_clocksource *cst;

    if (!cs)
    {
        return -1;
    }

    cst   = NULL;
    found = false;

    spin_lock_irqsave (&csctrl.lock, flags);

    list_for_each_entry (cst, &csctrl.clksrc_list, head)
    {
        if (strcmp (cst->name, cs->name) == 0)
        {
            found = true;
            break;
        }
    }

    if (found)
    {
        spin_unlock_irqrestore (&csctrl.lock, flags);
        return -1;
    }

    INIT_LIST_HEAD (&cs->head);
    list_add_tail (&cs->head, &csctrl.clksrc_list);

    spin_unlock_irqrestore (&csctrl.lock, flags);

    return 0;
}

/**
 * @brief 注销一个时钟源
 *
 * 该函数注销一个时钟源
 *
 * @param[in] cs timer_clocksource结构体指针
 * @return 成功时返回0，失败时返回-1
 */
int clocksource_unregister (struct timer_clocksource *cs)
{
    bool                      found;
    irq_flags_t               flags;
    struct timer_clocksource *cst;

    if (!cs)
    {
        return -1;
    }

    spin_lock_irqsave (&csctrl.lock, flags);

    if (list_empty (&csctrl.clksrc_list))
    {
        spin_unlock_irqrestore (&csctrl.lock, flags);
        return -1;
    }

    cst   = NULL;
    found = false;
    list_for_each_entry (cst, &csctrl.clksrc_list, head)
    {
        if (strcmp (cst->name, cs->name) == 0)
        {
            found = true;
            break;
        }
    }

    if (!found)
    {
        spin_unlock_irqrestore (&csctrl.lock, flags);
        return -1;
    }

    list_del (&cs->head);

    spin_unlock_irqrestore (&csctrl.lock, flags);

    return 0;
}

/**
 * @brief 选择最佳时钟源
 *
 * 该函数选择最佳时钟源
 *
 * @return 成功时返回时钟源，失败时返回NULL
 */
struct timer_clocksource *clocksource_best (void)
{
    int                       rating = 0;
    irq_flags_t               flags;
    struct timer_clocksource *cs, *best_cs;

    cs      = NULL;
    best_cs = NULL;

    spin_lock_irqsave (&csctrl.lock, flags);

    list_for_each_entry (cs, &csctrl.clksrc_list, head)
    {
        if (cs->rating > rating)
        {
            best_cs = cs;
            rating  = cs->rating;
        }
    }

    spin_unlock_irqrestore (&csctrl.lock, flags);

    return best_cs;
}

/**
 * @brief 根据name查找时钟源
 *
 * 该函数根据name查找时钟源
 *
 * @param[in] name 时钟源名字
 * @return 成功时返回时钟源，失败时返回NULL
 */
struct timer_clocksource *clocksource_find (const char *name)
{
    bool                      found;
    irq_flags_t               flags;
    struct timer_clocksource *cs;

    if (!name)
    {
        return NULL;
    }

    found = false;
    cs    = NULL;

    spin_lock_irqsave (&csctrl.lock, flags);

    list_for_each_entry (cs, &csctrl.clksrc_list, head)
    {
        if (strcmp (cs->name, name) == 0)
        {
            found = true;
            break;
        }
    }

    spin_unlock_irqrestore (&csctrl.lock, flags);

    if (!found)
    {
        return NULL;
    }

    return cs;
}

/**
 * @brief 根据索引查找时钟源
 *
 * 该函数根据索引查找时钟源
 *
 * @param[in] index 时钟源索引
 * @return 成功时返回时钟源，失败时返回NULL
 */
struct timer_clocksource *clocksource_get (int index)
{
    bool                      found;
    irq_flags_t               flags;
    struct timer_clocksource *cs;

    if (index < 0)
    {
        return NULL;
    }

    cs    = NULL;
    found = false;

    spin_lock_irqsave (&csctrl.lock, flags);

    list_for_each_entry (cs, &csctrl.clksrc_list, head)
    {
        if (!index)
        {
            found = true;
            break;
        }

        index--;
    }

    spin_unlock_irqrestore (&csctrl.lock, flags);

    if (!found)
    {
        return NULL;
    }

    return cs;
}

/**
 * @brief 获取时钟源个数
 *
 * 该函数获取时钟源个数
 *
 * @return 返回时钟源个数
 */
u32 clocksource_count (void)
{
    u32                       retval = 0;
    irq_flags_t               flags;
    struct timer_clocksource *cs;

    spin_lock_irqsave (&csctrl.lock, flags);

    list_for_each_entry (cs, &csctrl.clksrc_list, head)
    {
        retval++;
    }

    spin_unlock_irqrestore (&csctrl.lock, flags);

    return retval;
}

