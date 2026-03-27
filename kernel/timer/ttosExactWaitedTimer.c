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
/************************实    现******************************/

/* @MODULE> */

/*
 * @brief:
 *    将定时器从时间等待队列中移出。
 * @param[in]: timer: 定时器
 * @return:
 *    无
 * @tracedREQ: RT.4
 * @implements: DT.4.8
 */
void ttosExactWaitedTimer (T_TTOS_TimerControlBlock *timer)
{
    struct list_node *node, *next;
    /* 从差分时间等待队列移出时，如果有后继节点则必须更新后继节点的等待时间 */
    /* @KEEP_COMMENT: 获取定时器节点，记录为node */
    node = &(timer->objCore.objectNode);
    /*
     * @KEEP_COMMENT: 调用list_next(DT.8.10)获取定时器等待队列
     * ttosManager.timerWaitedQueue中node下一个定时器节点，记录为next
     */

    next = list_next(node, &(ttosManager.timerWaitedQueue));

    /* @REPLACE_BRACKET: next存在 */
    if (next != NULL)
    {
        /* @KEEP_COMMENT: next等待时间 = next等待时间 + node等待时间 */
        ((T_TTOS_TimerControlBlock *)next)->waitedTicks += timer->waitedTicks;
    }

    /* @KEEP_COMMENT: 设置node等待时间为0 */
    timer->waitedTicks = 0U;
    /* @KEEP_COMMENT: 调用list_delete(DT.8.8)将node从定时器等待队列移除
     */
    list_delete(node);
}
