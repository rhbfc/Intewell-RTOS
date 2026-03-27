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
#include <ttosUtils.inl>
/* @MOD_HEAD> */

/************************宏 定 义******************************/
/************************类型定义******************************/
/************************外部声明******************************/
/* @MOD_EXTERN> */
/* @MOD_EXTERN> */
/************************前向声明******************************/
/************************模块变量******************************/
/************************全局变量******************************/
/************************实    现******************************/

/* @MODULE> */

/*
 * @brief:
 *    实现定时器等待队列的时间通知。
 * @param[in]: ticks: 需要通知的tick数
 * @return:
 *    无
 * @implements: DT.4.19
 */
void ttosTimerNotify (T_UWORD ticks)
{
    TBSP_MSR_TYPE             msr;
    T_TTOS_TimerControlBlock *timer;
    struct list_node         *node;
    /* @KEEP_COMMENT: 禁止虚拟中断 */
    TBSP_GLOBALINT_DISABLE (msr);

    /* ---------------处理时间等待的定时器----------------- */

    /* @KEEP_COMMENT: 处理后续等待时间为0的节点 */
    /* @REPLACE_BRACKET: 1 */
    while (1)
    {
        /*
         * @KEEP_COMMENT: 调用list_first(DT.8.9)获取定时器等待队列
         * ttosManager.timerWaitedQueue中第一个定时器保存至节点node
         */
        node = list_first(&(ttosManager.timerWaitedQueue));

        /* @REPLACE_BRACKET: node为NULL */
        if (NULL == node)
        {
            break;
        }

        /* @KEEP_COMMENT: 设置定时器为node */
        timer = (T_TTOS_TimerControlBlock *)node;

        /* @REPLACE_BRACKET: 定时器等待tick数大于<ticks> */
        if (timer->waitedTicks > ticks)
        {
            /* @KEEP_COMMENT: 定时器等待tick数 = 定时器等待tick数 - <ticks> */
            timer->waitedTicks -= ticks;
        }

        else
        {
            /*
             * @KEEP_COMMENT: <ticks> = <ticks> - 定时器等待tick数，
             * 并设置定时器等待tick数为0
             */
            ticks -= timer->waitedTicks;
            timer->waitedTicks = 0U;
        }

        /* @REPLACE_BRACKET:定时器等待tick数不为0 */
        if (timer->waitedTicks != 0U)
        {
            break;
        }

        /*
         * @KEEP_COMMENT: 调用list_delete(DT.8.8)将node从定时器等待
         * 队列移除
         */
        list_delete(node);

        /* 检查是否需要重复激活 */
        /* @REPLACE_BRACKET: 定时器当前还能够触发的次数不为0 */
        if (0U != timer->remainCount)
        {
            /*
             *@KEEP_COMMENT: 设置定时器等待tick数为定时器的定时时间
             *，此处加上ticks是为了避免一次性
             *投入的ticks过多时，定时器不停的触发。
             */
            timer->waitedTicks = timer->ticks + ticks;
            /*
             * @KEEP_COMMENT: 调用ttosInsertWaitedTimer(DT.4.9)将定时器插入
             * 定时器等待队列
             */
            (void)ttosInsertWaitedTimer (timer);

            /* @REPLACE_BRACKET: 定时器可触发次数不为TTOS_TIMER_LOOP_FOREVER */
            if (timer->repeatCount != TTOS_TIMER_LOOP_FOREVER)
            {
                /* @KEEP_COMMENT: 定时器当前还能够触发的次数递减1 */
                timer->remainCount--;
            }
        }

        else
        {
            /* @KEEP_COMMENT: 设置定时器状态为TTOS_TIMER_STOP */
            timer->state = (T_UWORD)TTOS_TIMER_STOP;
        }

        /* 调用定时器处理程序 */
        /* @KEEP_COMMENT: 使能虚拟中断 */
        TBSP_GLOBALINT_ENABLE (msr);
        (timer->handler) (timer->timerControlId, timer->argument);
        /* @KEEP_COMMENT: 禁止虚拟中断 */
        TBSP_GLOBALINT_DISABLE (msr);
    }

    /* @KEEP_COMMENT: 使能虚拟中断 */
    TBSP_GLOBALINT_ENABLE (msr);
}
