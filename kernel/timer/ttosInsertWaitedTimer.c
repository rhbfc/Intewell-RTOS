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
 *    将定时器插入等待队列。
 * @param[out]||[in]: timer: 定时器
 * @return:
 *    无
 * @tracedREQ: RT.4
 * @implements: DT.4.9
 */
void ttosInsertWaitedTimer (T_TTOS_TimerControlBlock *timer)
{
    struct list_node         *node;
    T_TTOS_TimerControlBlock *timerWaited;
    /*
     * @KEEP_COMMENT: 调用list_first(DT.8.9)获取定时器等待队列
     * ttosManager.timerWaitedQueue中第一个定时器，记录为node
     */
    node = list_first(&(ttosManager.timerWaitedQueue));

    /* 采用差分等待队列 */
    /*
     * (1)如果定时器等待队列中没有定时器节点则则将<timer>节点添加到队尾；
     * (2)如果定时器等待队列中有定时器节点，则从第一个节点开始依次取出每个
     * 定时器节点，进行如下操作:(a)如果<timer>的等待时间大于该节点的等待时间，
     * 则将<timer>的等待时间减去该节点的等待时间后，重复比较定时器等待队列中的
     * 下一节点；(b)如果<timer>的等待时间小于等于该节点的等待时间，则将该节点的
     * 等待时间减去<timer>的等待时间后，将<timer>节点插入到该节点的前面，并返回；
     * (c)如果操作到队尾，则将<timer>节点添加到队尾，并返回。
     */
    /* @REPLACE_BRACKET: node不为NULL */
    while (node != NULL)
    {
        timerWaited = (T_TTOS_TimerControlBlock *)node;

        /* @REPLACE_BRACKET: <timer>等待tick数大于node等待tick数 */
        if (timer->waitedTicks > timerWaited->waitedTicks)
        {
            /* @KEEP_COMMENT: <timer>等待tick数 = <timer>等待tick数 -
             * node等待tick数 */
            timer->waitedTicks -= timerWaited->waitedTicks;
            /*
             * @KEEP_COMMENT: 调用list_next(DT.8.10)获取定时器等待队列
             * ttosManager.timerWaitedQueue中node下一个定时器，记录为node
             */
            node = list_next(node, &(ttosManager.timerWaitedQueue));
            continue;
        }

        else
        {
            /* @KEEP_COMMENT: node的等待tick数 = node等待tick数 -
             * <timer>等待tick数 */
            timerWaited->waitedTicks -= timer->waitedTicks;

            /* @KEEP_COMMENT:
             * 调用list_add_after(DT.8.11)将<timer>插入到node后面 */
            list_add_after((struct list_node *)timer, node->prev);
            break;
        }
    }

    /* 需要插入在队列尾部 添加节点到链表尾部。 */
    /* @REPLACE_BRACKET: node不存在 */
    if (NULL == node)
    {
        /*
         * @KEEP_COMMENT:
         * 调用list_add_tail(DT.8.7)将<timer>添加到定时器等待队列
         * ttosManager.timerWaitedQueue的尾部
         */
        list_add_tail((struct list_node *)timer, &(ttosManager.timerWaitedQueue));
    }
}
