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
 *    将周期任务从指定的周期等待队列或者截止期等待队列中移出。
 * @param[in]: chain: 队列控制块
 * @param[in]: time: 队列已经等待的时间
 * @param[out]: task: 任务控制块
 * @return:
 *    队列被通知时间。
 * @implements: RTE_DTASK.9.1
 */
T_UWORD ttosExactWaitedPeriodTask (T_TTOS_PeriodTaskNode *task,
                                   struct list_head   *chain,
                                   T_UWORD                waited_time)
{
    struct list_node *node, *next;
    T_UWORD           newTime;
    newTime = 0U;
    /* @KEEP_COMMENT: 获取<task>任务 */
    node = &(task->objectNode);
    /* @KEEP_COMMENT: 获取<task>任务在队列<chain>中的后继任务 */
    next = list_next(node, chain);

    /* @REPLACE_BRACKET: <task>任务在队列<chain>中的后继任务存在 */
    if (next != NULL)
    {
        /* @KEEP_COMMENT:
         * 后继任务在<chain>中的等待时间累加上<task>任务的等待时间 */
        ((T_TTOS_PeriodTaskNode *)next)->waitedTime += task->waitedTime;

        /* @REPLACE_BRACKET: <task>任务在队列<chain>中是第一个节点 */
        if (node == list_first(chain))
        {
            /* @KEEP_COMMENT: 后继任务的等待时间减去队列已经等待的时间<time> */
            ((T_TTOS_PeriodTaskNode *)next)->waitedTime -= waited_time;
            /* @KEEP_COMMENT: 保存后继任务的等待时间，存放至变量newTime */
            newTime = ((T_TTOS_PeriodTaskNode *)next)->waitedTime;
        }
    }

    task->waitedTime = 0U;
    /* @KEEP_COMMENT: 调用list_delete(DT.8.8)将<task>从<chain>队列移除
     */
    list_delete(node);
    /* @REPLACE_BRACKET: newTime */
    return (newTime);
}
