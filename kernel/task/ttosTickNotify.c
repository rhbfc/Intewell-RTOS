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

/* @<MODULE */

/************************头 文 件******************************/

/* @<MOD_HEAD */
#include <ttosBase.h>
#include <ttosInterHal.h>
#include <ttosInterTask.inl>
#include <ttosUtils.inl>
/* @MOD_HEAD> */

/************************宏 定 义******************************/
/************************类型定义******************************/
/************************外部声明******************************/

/* @<MOD_EXTERN */

/* @MOD_EXTERN> */

/************************前向声明******************************/
/************************模块变量******************************/
/************************全局变量******************************/
/************************实   现*******************************/

/* @MODULE> */

/*
 * @brief:
 *    实现任务的tick时间通知，包括任务tick等待队列和任务时间片轮转调度的时间通知。
 * @param[in]: ticks: 欲通知的tick数
 * @return:
 *    无
 * @tracedREQ: RT.2
 * @implements: RTE_DTASK.41.1
 */
void ttosTickNotify (T_UWORD ticks)
{
    TBSP_MSR_TYPE            msr;
    T_TTOS_TaskControlBlock *task;
    struct list_node        *node;
    /* @KEEP_COMMENT: 设置变量ticksTmp为<ticks> */
    T_UWORD ticksTmp = ticks;
    T_UWORD resetTickSlice;
    T_UWORD state = (T_UWORD)TTOS_TASK_BLOCKING;

    msr = 0;

    /* @KEEP_COMMENT: 禁止虚拟中断 */
    TBSP_GLOBALINT_DISABLE (msr);

    /* 处理时间等待的任务 */
    /* @REPLACE_BRACKET: 1 */
    while (1)
    {
        /*
         * @KEEP_COMMENT: 调用list_first(DT.8.9)获取tick等待队列
         * ttosManager.taskWaitedQueue中第一个任务存放至变量node
         */
        node = list_first(&(ttosManager.taskWaitedQueue));

        /* @REPLACE_BRACKET: node任务不存在 */
        if (NULL == node)
        {
            break;
        }

        /* @KEEP_COMMENT: node任务存放至变量task */
        task = (T_TTOS_TaskControlBlock *)node;

        /* @REPLACE_BRACKET: task任务还需要等待的tick数大于ticksTmp */
        if (task->waitedTicks > ticksTmp)
        {
            /* @KEEP_COMMENT: task任务还需要等待的tick数减ticksTmp */
            task->waitedTicks -= ticksTmp;
        }

        else
        {
            /* @KEEP_COMMENT: ticksTmp减去task任务还需要等待的tick数 */
            ticksTmp -= task->waitedTicks;
            /* @KEEP_COMMENT: 设置task任务还需要等待的tick数为零 */
            task->waitedTicks = 0U;
        }

        /* @REPLACE_BRACKET: task任务还需要等待的tick数不为零 */
        if (task->waitedTicks != 0U)
        {
            break;
        }

        /* @KEEP_COMMENT:
         * 调用list_delete(DT.8.8)将task任务从tick等待队列移除 */
        list_delete(node);
        (void)ttosExtractTaskq (task->wait.queue, task);

        /* @REPLACE_BRACKET: U(0) != (task->state & TTOS_TASK_BLOCKING) */
        if (0U != (task->state & state))
        {
            /* 任务等待资源时超时了*/
            task->wait.returnCode = TTOS_TIMEOUT;
        }

        /* @KEEP_COMMENT:
         * 调用ttosClearTaskWaiting(DT.2.22)清除task任务的等待状态 */
        (void)ttosClearTaskWaiting (task);
    }

    /* 任务时间片轮转 */
    T_UWORD i;

    /* @REPLACE_BRACKET: i = U(0); i < TTOS_CONFIG_CPUS(); i ++ */
    for (i = 0U; i < TTOS_CONFIG_CPUS (); i++)
    {
        /* @REPLACE_BRACKET: U(0) == CPUSET_ISSET(TTOS_CPUSET_ENABLED(), i) */
        if (0U == CPU_ISSET (i, TTOS_CPUSET_ENABLED ()))
        {
            /*  指定的cpu是否使能 */
            continue;
        }

        /* @KEEP_COMMENT: 获取指定CPU上正在运行的任务存放至变量task */
        task = ttosGetRunningTaskWithCpuID (i);
        /* @KEEP_COMMENT: 任务执行的tick数加1 */
        (task->executedTicks)++;

        /* @REPLACE_BRACKET: TTOS_TASK_READY != (task->state & TTOS_TASK_READY)
         */
        state = (T_UWORD)TTOS_TASK_READY;
        if (state != (task->state & state))
        {
            /*
             *当前运行任务在删除自身或者自身退出或者等待资源时，当前执行任务已经不在就绪链表上，
             *使能中断时，如果此时来了定时器中断，有可能会执行到此分支。
             */
            continue;
        }

        /* @REPLACE_BRACKET: task任务不允许抢占 */
        if (task->preemptCount > 0U)
        {
            /* @KEEP_COMMENT: 使能虚拟中断 */
            continue;
        }

        resetTickSlice = task->tickSliceSize;

        if (SCHED_FIFO == task->taskSchedPolicy)
        {
            continue;
        }

        /* @REPLACE_BRACKET: task任务当前时间片大于0 */
        if (task->tickSlice > 0U)
        {
            /* @KEEP_COMMENT: task任务当前时间片减1 */
            task->tickSlice--;
        }

        /* @REPLACE_BRACKET: task任务当前时间片为零 */
        if (0U == task->tickSlice)
        {
            /* @KEEP_COMMENT: 复位task任务的时间片 */
            task->tickSlice = resetTickSlice;
            (void)ttosRotateRunningTask (task);
        }
    }

    /* @KEEP_COMMENT: 使能虚拟中断 */
    TBSP_GLOBALINT_ENABLE (msr);
}
