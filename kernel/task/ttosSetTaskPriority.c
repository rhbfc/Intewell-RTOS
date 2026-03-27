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

/**
 * @brief:
 *    获取指定任务的优先级。
 * @param[in]: taskID: 任务ID，当为TTOS_SELF_OBJECT_ID时表示获取当前任务的优先级
 * @param[out]: taskPriority: 存放任务优先级
 * @return:
 *    TTOS_INVALID_ID：<taskID>所指定的对象类型不是任务或不存在。
 *    TTOS_INVALID_ADDRESS：<taskPriority>为NULL。
 *    TTOS_OK：获取任务的优先级成功。
 * @implements: RTE_DTASK.19.1
 */
/*
 * @brief
 *       设置指定任务的优先级。
 * @param[in]      theTask: 待改变的任务控制块
 * @param[in]  newPriority: 新的任务优先级
 * @param[in]    prependIt: 任务是否优先考虑
 * @return
 *     无
 * @implements: RTE_DTASK.40.12
 */
T_TTOS_ReturnCode TTOS_SetTaskPriority (T_TTOS_TaskControlBlock *theTask,
                                        T_UBYTE newPriority, T_BOOL prependIt)
{
    TBSP_MSR_TYPE msr            = 0U;
    T_UBYTE       beforePriority = theTask->taskCurPriority;
    T_UWORD       state          = (T_UWORD)TTOS_TASK_NONRUNNING_STATE;
    /*此接口仅仅使用于任务优先级改变时，并没有在优先级等待队列上。*/
    /*定时器中断处理程序中有可能要操作就绪队列，所以此处需要关中断。*/
    TBSP_GLOBALINT_DISABLE (msr);
    /* @KEEP_COMMENT: 设置当前使用优先级*/
    theTask->taskCurPriority = theTask->taskPriority = newPriority;

    /* @REPLACE_BRACKET: <task>任务状态不是TTOS_TASK_NONRUNNING_STATE */
    if (0 == (theTask->state & state))
    {
#if defined(CONFIG_SMP)
        /* 非绑定的运行任务是从就绪队列摘除了的*/
        /* @REPLACE_BRACKET: (CPU_NONE != theTask->smpInfo.affinityCpuIndex) ||
         * (CPU_NONE == theTask->smpInfo.cpuIndex)*/
        if ((CPU_NONE != theTask->smpInfo.affinityCpuIndex)
            || (CPU_NONE == theTask->smpInfo.cpuIndex))
        {
#endif
            /* 绑定的就绪任务或者非绑定非运行任务是才需要从就绪队列摘除*/
            /* @KEEP_COMMENT: 将任务从以前的优先级链表中移除 */
            list_delete(&(theTask->objCore.objectNode));
            /* 优化会导致指令重新排序，添加编译屏障保证顺序排列指令 */
            TTOS_COMPILER_MEMORY_BARRIER ();

            /* @REPLACE_BRACKET: TRUE ==
             * ttosReadyQueueIsEmpty(theTask->smpInfo.affinityCpuIndex,
             * beforePriority)*/
            if (TRUE
                == ttosReadyQueueIsEmpty (theTask->smpInfo.affinityCpuIndex,
                                          beforePriority))
            {
                /* @KEEP_COMMENT:
                 * 清除<task>任务的优先级在优先级位图中ttosPriorityBitMap对应的位
                 */
                TBSP_CLEAR_PRIORITYBITMAP (theTask->smpInfo.affinityCpuIndex,
                                           beforePriority);
            }

            /* @KEEP_COMMENT: 修改优先级位图 */
            TBSP_SET_PRIORITYBITMAP (theTask->smpInfo.affinityCpuIndex,
                                     newPriority);

            /* @REPLACE_BRACKET: prependIt == FALSE */
            if (prependIt == FALSE)
            {
                /* @KEEP_COMMENT:
                 * 将<task>任务插入对应优先级就绪队列ttosManager.priorityQueue尾部
                 */
                ttosReadyQueuePut (theTask, FALSE);
            }

            else
            {
                /* @KEEP_COMMENT:
                 * 将<task>任务插入对应优先级就绪队列ttosManager.priorityQueue头部
                 */
                ttosReadyQueuePut (theTask, TRUE);
            }

            /* @KEEP_COMMENT: 获取当前优先级最高的任务 */
            ttosSetHighestPriorityTask (theTask->smpInfo.affinityCpuIndex);
#if defined(CONFIG_SMP)
        }

#endif
        (void)ttosTaskStateChanged (theTask, TRUE);
    }

    /* @KEEP_COMMENT: 恢复中断 */
    TBSP_GLOBALINT_ENABLE (msr);

    return TTOS_OK;
}
