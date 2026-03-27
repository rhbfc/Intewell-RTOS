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

#include <restart.h>
#include <time.h>
#include <time/ktimer.h>
#include <ttos.h>
#include <ttosBase.h>
#include <ttosInterHal.h>
#include <ttosProcess.h>
#include <util.h>

#define KTIMER_SLEEP_TASK_STATE (TTOS_TASK_INTERRUPTIBLE_BY_SIGNAL | TTOS_KTIMER_SLEEP)

/* ==================== 休眠与重启支持 ==================== */

static void wake_sleeping_task(T_TTOS_TaskControlBlock *task)
{
    irq_flags_t flags;

    ttosDisableTaskDispatchWithLock();
    TBSP_GLOBALINT_DISABLE(flags);

    if (!task || !(task->state & TTOS_KTIMER_SLEEP) || (task->state & TTOS_TASK_READY))
    {
        TBSP_GLOBALINT_ENABLE(flags);
        ttosEnableTaskDispatchWithLock();
        return;
    }

    task->state &= ~KTIMER_SLEEP_TASK_STATE;
    task->wait.returnCode = TTOS_TIMEOUT;

    ttosSetTaskReady(task);

    TBSP_GLOBALINT_ENABLE(flags);
    ttosEnableTaskDispatchWithLock();
}

static enum ktimer_restart ktimer_wakeup_sleep_ctx(struct ktimer *timer)
{
    struct ktimer_sleep_ctx  *sleep_ctx = container_of(timer, struct ktimer_sleep_ctx , timer);
    T_TTOS_TaskControlBlock *task = sleep_ctx->task;

    sleep_ctx->task = NULL;
    if (task && !(task->state & TTOS_TASK_READY))
        wake_sleeping_task(task);

    return KTIMER_NORESTART;
}

static void ktimer_init_sleep_ctx(struct ktimer_sleep_ctx *sleep_ctx, clockid_t clock_id,
                                enum ktimer_mode mode)
{
    /* 当前仅保留硬中断定时器，未显式指定时默认补上 HARD 标记。 */
    if (!(mode & KTIMER_MODE_HARD))
        mode |= KTIMER_MODE_HARD;

    ktimer_init(&sleep_ctx->timer, ktimer_wakeup_sleep_ctx, clock_id, mode);
    sleep_ctx->timer.function = ktimer_wakeup_sleep_ctx;
    sleep_ctx->task = TTOS_GetRunningTask();
}

void ktimer_sleep_ctx_start_expires(struct ktimer_sleep_ctx  *sleep_ctx, enum ktimer_mode mode)
{
    ktime_t time = ktimer_get_expires(&sleep_ctx->timer);

    ktimer_start(&sleep_ctx->timer, time, mode);
}

static void nanosleep_copyout_timespec(struct restart_info *restart, struct timespec64 *ts)
{
    struct timespec *timespec = (struct timespec *)restart->nanosleep.rmtp;

    /* restart->nanosleep.rmtp 为用户系统调用传入的指针 */
    timespec->tv_nsec = ts->tv_nsec;
    timespec->tv_sec = ts->tv_sec;
}

static void nanosleep_copyout_timespec32(struct restart_info *restart, struct timespec64 *ts)
{
    struct timespec32 *timespec32 = (struct timespec32 *)restart->nanosleep.rmtp;

    /* restart->nanosleep.rmtp 为用户系统调用传入的指针 */
    timespec32->tv_nsec = ts->tv_nsec;
    timespec32->tv_sec = ts->tv_sec;
}

static void nanosleep_copyout_timespec64(struct restart_info *restart, struct timespec64 *ts)
{
    struct timespec64 *timespec64 = (struct timespec64 *)restart->nanosleep.rmtp;

    /* restart->nanosleep.rmtp 为用户系统调用传入的指针 */
    timespec64->tv_nsec = ts->tv_nsec;
    timespec64->tv_sec = ts->tv_sec;
}

static inline bool pcb_need_nanosleep_remain(pcb_t pcb)
{
    return pcb && pcb->restart.nanosleep.rmtp_exist != RMTP_NONE;
}

static void (*nanosleep_remain_copyouts[])(struct restart_info *restart, struct timespec64 *ts) = {
    [TIMESPEC] = nanosleep_copyout_timespec,
    [TIMESPEC32] = nanosleep_copyout_timespec32,
    [TIMESPEC64] = nanosleep_copyout_timespec64,
};

int32_t write_nanosleep_remain(struct restart_info *restart, struct timespec64 *ts)
{
    nanosleep_remain_copyouts[restart->nanosleep.rmtp_timespec_type](restart, ts);
    return -ERR_RESTART_WITH_BLOCK;
}

static int32_t wait_sleep_ctx_timeout_or_signal(struct ktimer_sleep_ctx  *sleep_ctx,
                                              enum ktimer_mode mode)
{
    irq_flags_t flags;
    T_TTOS_TaskControlBlock *task = ttosGetRunningTask();
    pcb_t pcb = (pcb_t)task->ppcb;

    TBSP_GLOBALINT_DISABLE_WITH_LOCK(flags);

    ktimer_sleep_ctx_start_expires(sleep_ctx, mode);
    ttosClearTaskReady(task);
    task->state |= KTIMER_SLEEP_TASK_STATE;

    TBSP_GLOBALINT_ENABLE_WITH_LOCK(flags);

    ttosSchedule();

    task->state &= ~KTIMER_SLEEP_TASK_STATE;

    /* 任务休眠醒来 */
    ktimer_cancel(&sleep_ctx->timer);

    if (!sleep_ctx->task)
    {
        /* 休眠超时返回 */
        return 0;
    }

    /* 被信号打断返回 */
    if (pcb_need_nanosleep_remain(pcb))
    {
        ktime_t rem = ktimer_expires_remaining(&sleep_ctx->timer);
        struct timespec64 rmt;

        if (rem <= 0)
            return 0;

        rmt = ktime_to_timespec64(rem);
        return write_nanosleep_remain(&pcb->restart, &rmt);
    }

    return -ERR_RESTART_WITH_BLOCK;
}

static long ktimer_nanosleep_restart(struct restart_info *restart)
{
    struct ktimer_sleep_ctx  sleep_ctx;
    int32_t ret;

    ktimer_init_sleep_ctx(&sleep_ctx, restart->nanosleep.clockid, KTIMER_MODE_ABS);
    ktimer_set_expires(&sleep_ctx.timer, restart->nanosleep.expires);
    ret = wait_sleep_ctx_timeout_or_signal(&sleep_ctx, KTIMER_MODE_ABS);

    return ret;
}

long ktimer_nanosleep(ktime_t rqtp, const enum ktimer_mode mode, const clockid_t clock_id)
{
    int32_t ret = 0;
    struct restart_info *restart = NULL;
    struct ktimer_sleep_ctx  sleep_ctx = {0};
    T_TTOS_TaskControlBlock *task = ttosGetRunningTask();

    if (!task)
        return -1;

    ktimer_init_sleep_ctx(&sleep_ctx, clock_id, mode);
    ktimer_set_expires(&sleep_ctx.timer, rqtp);
    ret = wait_sleep_ctx_timeout_or_signal(&sleep_ctx, mode);
    if (ret != -ERR_RESTART_WITH_BLOCK)
    {
        task->wait.returnCode = TTOS_TIMEOUT;
        return ret;
    }

    /* 被信号打断提前唤醒 */
    if (mode == KTIMER_MODE_ABS)
        return -ERR_RESTART_IF_NO_HANDLER;

    /* 使用相对模式休眠,则需要使用 restart_block + restart_syscall 方式重启 */
    if (task->ppcb)
    {
        pcb_t pcb = (pcb_t)task->ppcb;

        restart = &pcb->restart;
        restart->nanosleep.clockid = sleep_ctx.timer.base->clock_id;
        restart->nanosleep.expires = ktimer_get_expires(&sleep_ctx.timer);
        restart_set_func(restart, ktimer_nanosleep_restart);
    }

    return ret;
}
