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
#include <ttosUtils.inl>
/* @MOD_HEAD> */

/************************宏 定 义******************************/
/************************类型定义******************************/
/************************外部声明******************************/
/************************前向声明******************************/
/************************模块变量******************************/
/************************全局变量******************************/
/************************实   现*******************************/

/* @MODULE> */

/*
 * @brief:
 *    将周期任务插入指定的周期等待队列或者截止期等待队列。
 * @param[in]: chain: 队列控制块
 * @param[in]: waited_time: 队列已经等待的时间
 * @param[in]: task: 任务控制块
 * @return:
 *    队列被通知时间。
 * @implements: RTE_DTASK.23.1
 */
T_UWORD ttosInsertWaitedPeriodTask (T_TTOS_PeriodTaskNode *task,
                                    struct list_head   *chain,
                                    T_UWORD                waited_time)
{
    struct list_node      *node, *oldFirst;
    T_TTOS_PeriodTaskNode *taskWaited;
    /* @KEEP_COMMENT: 获取<chain>队列上第一个任务存放至变量node */
    node     = list_first(chain);
    oldFirst = node;

    /* @REPLACE_BRACKET: node不为空 */
    if (node != NULL)
    {
        /* @KEEP_COMMENT: <chain>队列上第一个任务的等待时间减少<time>时间 */
        ((T_TTOS_PeriodTaskNode *)node)->waitedTime -= waited_time;
    }

    /* @REPLACE_BRACKET: node不为空 */
    while (node != NULL)
    {
        taskWaited = (T_TTOS_PeriodTaskNode *)node;

        /* @REPLACE_BRACKET: task任务的等待时间大于node任务的等待时间 */
        if (task->waitedTime >= taskWaited->waitedTime)
        {
            /* 等待时间是累加,此时需要减去前任务的等待时间 */
            /* @KEEP_COMMENT: task任务的等待时间减去node任务的等待时间 */
            task->waitedTime -= taskWaited->waitedTime;
            /* @KEEP_COMMENT: 获取node任务的后继任务存放至node */
            node = list_next(node, chain);
            continue;
        }

        else
        {
            /* @KEEP_COMMENT: node任务的等待时间减去task任务的等待时间 */
            taskWaited->waitedTime -= task->waitedTime;
            /* @KEEP_COMMENT:
             * 调用list_add_after(DT.8.11)将<task>任务插入node任务的后面 */
            list_add_after((struct list_node *)task, node->prev);
            break;
        }
    }

    /* @REPLACE_BRACKET: node为NULL */
    if (NULL == node)
    {
        /* @KEEP_COMMENT:
         * 调用list_add_tail(DT.8.7)将<task>任务插入<chain>队列尾部 */
        list_add_tail((struct list_node *)task, chain);
    }

    /* @KEEP_COMMENT: 获取<chain>队列上第一个任务存放至变量node */
    node = list_first(chain);

    /* @REPLACE_BRACKET: node等于<task> */
    if (((T_TTOS_PeriodTaskNode *)node) == task)
    {
        /* @REPLACE_BRACKET: <task>任务的等待时间 */
        return (task->waitedTime);
    }

    /* @REPLACE_BRACKET: oldFirst不为NULL */
    if (oldFirst != NULL)
    {
        /* @KEEP_COMMENT: node任务的等待时间累加上<time> */
        ((T_TTOS_PeriodTaskNode *)oldFirst)->waitedTime += waited_time;
    }
    /* @REPLACE_BRACKET: 0 */
    return (0);
}
