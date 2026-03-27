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

#include <time/ktimer.h>
#include <restart.h>
#include <ttos_time.h>
#include <ttosHal.h>
#include <klog.h>
#include <limits.h>
#include <util.h>

#define USLEEP_MAX_USEC		UINT_MAX
#define MSLEEP_MAX_SLICE_MSEC	(USLEEP_MAX_USEC / USEC_PER_MSEC)

static bool sleep_in_interrupt_context(void)
{
    if (!ttosIsISR())
    {
        return false;
    }

    KLOG_EMERG("FAILED! sleep called in interrupt context!!!");
    return true;
}

static unsigned long sleep_elapsed_usecs(u64 start_ns)
{
    u64 now_ns = ttos_time_walltime_get();

    if (now_ns <= start_ns)
    {
        return 0;
    }

    return DIV_ROUND_UP(now_ns - start_ns, NSEC_PER_USEC);
}

static unsigned long sleep_remaining_usecs(unsigned long useconds, u64 start_ns)
{
    unsigned long elapsed = sleep_elapsed_usecs(start_ns);

    /* 剩余时间对外以微秒返回，向上取整后这里直接做饱和扣减即可。 */
    if (elapsed >= useconds)
    {
        return 0;
    }

    return useconds - elapsed;
}

static bool sleep_was_interrupted(long ret)
{
    return ret == -ERR_RESTART_WITH_BLOCK ||
           ret == -ERR_RESTART_IF_NO_HANDLER;
}

/* 可中断usleep */
unsigned long usleep_interruptible (unsigned useconds)
{
    long ret = 0;
    u64 start_ns;

    if (sleep_in_interrupt_context())
    {
        return useconds;
    }

    start_ns = ttos_time_walltime_get();
    ret = ktimer_nanosleep((u64)useconds * NSEC_PER_USEC, KTIMER_MODE_REL, CLOCK_MONOTONIC);
    if (ret == 0)
    {
        return 0;
    }

    if (sleep_was_interrupted(ret))
    {
        return sleep_remaining_usecs(useconds, start_ns);
    }

    return useconds;
}

/* 可中断msleep */
unsigned long msleep_interruptible(unsigned int msecs)
{
    unsigned long remain_ms = msecs;

    while (remain_ms)
    {
        unsigned long slice_ms = remain_ms;
        unsigned long remain_us;

        /* usleep接口参数是unsigned，长时间睡眠需要拆分成多个安全片段。 */
        if (slice_ms > MSLEEP_MAX_SLICE_MSEC)
        {
            slice_ms = MSLEEP_MAX_SLICE_MSEC;
        }

        remain_us = usleep_interruptible(slice_ms * USEC_PER_MSEC);
        if (remain_us)
        {
            return (remain_ms - slice_ms) +
                   DIV_ROUND_UP(remain_us, USEC_PER_MSEC);
        }

        remain_ms -= slice_ms;
    }

    return 0;
}

/* 不可中断usleep */
int usleep (unsigned useconds)
{
    long ret = 0;

    if (sleep_in_interrupt_context())
    {
        return useconds;
    }

    while (useconds)
    {
        unsigned long remain;
        u64 start_ns = ttos_time_walltime_get();

        ret = ktimer_nanosleep((u64)useconds * NSEC_PER_USEC, KTIMER_MODE_REL, CLOCK_MONOTONIC);
        if (ret == 0)
        {
            break;
        }

        /* 非可重启类错误直接返回，避免把真实异常当成信号打断反复重试。 */
        if (!sleep_was_interrupted(ret))
        {
            return ret;
        }

        remain = sleep_remaining_usecs(useconds, start_ns);
        /* 极端情况下本轮换算结果可能没有前进，主动减1避免陷入死循环。 */
        if (remain == useconds)
        {
            remain--;
        }

        useconds = remain;
    }

    return 0;
}

/* 不可中断msleep */
void msleep (unsigned ms)
{
    while (ms)
    {
        unsigned int slice_ms = ms;

        /* 复用usleep的实现，长时间毫秒睡眠同样拆分成多个安全片段。 */
        if (slice_ms > MSLEEP_MAX_SLICE_MSEC)
        {
            slice_ms = MSLEEP_MAX_SLICE_MSEC;
        }

        if (usleep(slice_ms * USEC_PER_MSEC) < 0)
        {
            return;
        }

        ms -= slice_ms;
    }
}
