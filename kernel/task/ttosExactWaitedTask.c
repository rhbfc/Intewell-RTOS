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
#include <commonTypes.h>
#include <ttosBase.h>
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
 *    将任务从任务tick等待队列中移除。
 * @param[out]||[in]: task: 任务控制块
 * @return:
 *    无
 * @implements: RTE_DTASK.10.1
 */
void ttosExactWaitedTask (T_TTOS_TaskControlBlock *task)
{
    struct list_node *node, *next;
    node = &(task->objCore.objectNode);
    if (node == NULL)
    {
        return;
    }
    /* @KEEP_COMMENT:
     * 获取<task>任务在任务tick等待队列ttosManager.taskWaitedQueue中的后继任务
     */
    next = list_next(node, &(ttosManager.taskWaitedQueue));

    /* @REPLACE_BRACKET: <task>任务在任务tick等待队列中的后继任务存在 */
    if (next != NULL)
    {
        /* @KEEP_COMMENT: 后继任务的等待时间累加上<task>任务的等待时间 */
        ((T_TTOS_TaskControlBlock *)next)->waitedTicks += task->waitedTicks;
    }

    task->waitedTicks = 0U;
    /* @KEEP_COMMENT: 将task任务从任务tick等待队列中移除 */
    list_delete(node);
}
