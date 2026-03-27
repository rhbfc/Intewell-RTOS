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

#ifndef _CLOCKSOURCE_H__
#define _CLOCKSOURCE_H__

#include <clock/clockchip.h>
#include <list.h>
#include <system/types.h>

struct timer_clocksource;

/**
 * Hardware abstraction a timer subsystem clocksource
 * Provides mostly state-free accessors to the underlying hardware.
 * This is the structure used for tracking passsing time.
 *
 * @name:		ptr to clocksource name
 * @list:		list head for registration
 * @rating:		rating value for selection (higher is better)
 *			To avoid rating inflation the following
 *			list should give you a guide as to how
 *			to assign your clocksource a rating
 *			1-99: Unfit for real use
 *				Only available for bootup and testing purposes.
 *			100-199: Base level usability.
 *				Functional for real use, but not desired.
 *			200-299: Good.
 *				A correct and usable clocksource.
 *			300-399: Desired.
 *				A reasonably fast and accurate clocksource.
 *			400-499: Perfect
 *				The ideal clocksource. A must-use where
 *				available.
 * @read:		returns a cycle value, passes clocksource as argument
 * @enable:		optional function to enable the clocksource
 * @disable:		optional function to disable the clocksource
 * @mask:		bitmask for two's complement
 *			subtraction of non 64 bit counters
 * @freq:		frequency at which counter is running
 * @mult:		cycle to nanosecond multiplier
 * @shift:		cycle to nanosecond divisor (power of two)
 * @suspend:		suspend function for the clocksource, if necessary
 * @resume:		resume function for the clocksource, if necessary
 */
struct timer_clocksource {
    struct list_head head;
    const char      *name;
    int              rating;
    u64              mask;
    u32              freq;
    u32              mult;
    u32              shift;
    u64 (*read) (struct timer_clocksource *cs);
    int (*enable) (struct timer_clocksource *cs);
    void (*disable) (struct timer_clocksource *cs);
    void (*clocksource) (struct timer_clocksource *cs);
    void (*resume) (struct timer_clocksource *cs);
    void *priv;
};

/* simplify initialization of mask field */
#define CLOCKSOURCE_MASK(bits)                                             \
    (u64) ((bits) < 64 ? ((1ULL << (bits)) - 1) : -1)

/**
 * Layer above a %struct timer_clocksource which counts nanoseconds
 * Contains the state needed by timecounter_read() to detect
 * clocksource wrap around. Initialize with timecounter_init().
 * Users of this code are responsible for initializing the underlying
 * clocksource hardware, locking issues and reading the time more often
 * than the clocksource wraps around. The nanosecond counter will only
 * wrap around after ~585 years.
 *
 * @cs:			the cycle counter used by this instance
 * @cycles_last:	most recent cycle counter value seen by
 *			timecounter_read()
 * @nsec:		continuously increasing count
 */
struct timecounter {
    struct timer_clocksource *cs;
    u64                       cycles_last;
    u64                       nsec;
};

#endif
