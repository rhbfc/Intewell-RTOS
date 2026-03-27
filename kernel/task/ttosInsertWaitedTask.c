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

/* @<MOD_EXTERN */

/* @MOD_EXTERN> */

/************************前向声明******************************/
/************************模块变量******************************/
/************************全局变量******************************/
/************************实   现*******************************/

/* @MODULE> */

/*
 * @brief:
 *    将任务插入任务tick等待队列。
 * @param[out]||[in]: task: 任务控制块
 * @return:
 *    无
 * @implements: RTE_DTASK.24.1
 */
void ttosInsertWaitedTask (T_TTOS_TaskControlBlock *task)
{
    struct list_node        *node;
    T_TTOS_TaskControlBlock *taskWaited;
    /*
     * @KEEP_COMMENT:
     * 获取任务tick等待队列ttosManager.taskWaitedQueue第一个任务节点存放
     * 至变量node
     */
    node = list_first(&(ttosManager.taskWaitedQueue));

    /* @REPLACE_BRACKET: node不为空 */
    while (node != NULL)
    {
        taskWaited = (T_TTOS_TaskControlBlock *)node;

        /* @REPLACE_BRACKET: task任务的等待时间大于node任务的等待时间 */
        if (task->waitedTicks >= taskWaited->waitedTicks)
        {
            /* @KEEP_COMMENT: task任务的等待时间减去node任务的等待时间 */
            task->waitedTicks -= taskWaited->waitedTicks;
            /* @KEEP_COMMENT: 获取node任务的后继任务存放至node */
            node = list_next(node, &(ttosManager.taskWaitedQueue));
            continue;
        }

        else
        {
            /* @KEEP_COMMENT: node任务的等待时间减去task任务的等待时间 */
            taskWaited->waitedTicks -= task->waitedTicks;
            /* @KEEP_COMMENT:
             * 调用list_add_after(DT.8.11)将<task>任务插入node任务的后面 */
            list_add_after((struct list_node *)task, node->prev);
            break;
        }
    }

    /* @REPLACE_BRACKET: node为NULL */
    if (NULL == node)
    {
        /*
         * @KEEP_COMMENT:
         * 调用list_add_tail(DT.8.7)将<task>任务插入任务tick等待队列
         * ttosManager.taskWaitedQueue
         */
        list_add_tail((struct list_node *)task, &(ttosManager.taskWaitedQueue));
    }
}
