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
 *    清除任务的就绪状态。
 * @param[out]||[in]: task: 任务控制块
 * @return:
 *    无
 * @implements: RTE_DTASK.2.1
 */
void ttosClearTaskReady (T_TTOS_TaskControlBlock *task)
{
    T_UWORD state = (T_UWORD)TTOS_TASK_READY;
    state         = ~state;

    /* @KEEP_COMMENT: 清除<task>任务的TTOS_TASK_READY状态 */
    // task->state &= (~TTOS_TASK_READY);
    task->state &= state;
#if defined(CONFIG_SMP)
    /* @KEEP_COMMENT: 非绑定的运行任务是从就绪队列摘除了的*/
    /* CPU_NONE */
    /* @REPLACE_BRACKET: (CPU_NONE != task->smpInfo.affinityCpuIndex) ||
     * (CPU_NONE == task->smpInfo.cpuIndex) */
    if ((CPU_NONE != task->smpInfo.affinityCpuIndex)
        || (CPU_NONE == task->smpInfo.cpuIndex))
    {
#endif
        /* 绑定的就绪任务或者非绑定非运行任务是才需要从就绪队列摘除*/
        /* @KEEP_COMMENT:
         * 调用list_delete(DT.8.8)将<task>任务从任务优先级就绪队列中移除
         */
        list_delete(&(task->objCore.objectNode));
        /* 优化会导致指令重新排序，添加编译屏障保证顺序排列指令 */
        TTOS_COMPILER_MEMORY_BARRIER ();

        /* @REPLACE_BRACKET: <task>任务所在的优先级就绪队列为空 */
        if (TRUE
            == ttosReadyQueueIsEmpty (task->smpInfo.affinityCpuIndex,
                                      task->taskCurPriority))
        {
            /* @KEEP_COMMENT:
             * 清除<task>任务的优先级在优先级位图中ttosPriorityBitMap对应的位 */
            TBSP_CLEAR_PRIORITYBITMAP (task->smpInfo.affinityCpuIndex,
                                       task->taskCurPriority);
        }

        /* @KEEP_COMMENT:
         * 重新获取最高优先级任务，保存于ttosManager.priorityQueueTask */
        ttosSetHighestPriorityTask (task->smpInfo.affinityCpuIndex);
#if defined(CONFIG_SMP)
#if CONFIG_SMP == 1
    }

#endif
#endif
    /* 任务从就绪变为非就绪,检查是否需要对非当前CPU发送重调度IPI */
    (void)ttosTaskStateChanged (task, FALSE);
}
