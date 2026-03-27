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
#include <trace.h>
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
 *    设置任务为就绪任务。
 * @param[out]||[in]: task: 任务控制块
 * @return:
 *    无
 * @implements: RTE_DTASK.33.1
 */
void ttosSetTaskReady (T_TTOS_TaskControlBlock *task)
{
    if (task->state & (TTOS_TASK_READY | TTOS_TASK_SUSPEND))
    {
        return;
    }

    /* @KEEP_COMMENT: 将task任务的状态设置为当前状态与TTOS_TASK_READY的复合态 */
    task->state |= (T_UWORD) TTOS_TASK_READY;
    trace_waking_task(task);
    /*
     *绑定的运行任务变为非绑定的运行任务时，或者重启非绑定的运行任务时，不能插入
     *就绪队列，否则此任务可能同时在多个CPU上运行，系统的运行是不可预期的。
     *非绑定的运行任务的条件是((CPU_NONE == task->smpInfo.affinityCpuIndex
     *)&&(CPU_NONE != task->smpInfo.cpuIndex))
     */
#if defined(CONFIG_SMP)

    /* @REPLACE_BRACKET: 非绑定的运行任务是从就绪队列摘除了的*/
    if ((CPU_NONE != task->smpInfo.affinityCpuIndex)
        || (CPU_NONE == task->smpInfo.cpuIndex))
    {
#endif
        /*
         * @KEEP_COMMENT:
         * 设置<task>任务的优先级在优先级位图ttosPriorityBitMap中的相应位
         * 设置优先级位图，并将Task加入到其优先级所在的就绪队列尾部
         */
        TBSP_SET_PRIORITYBITMAP (task->smpInfo.affinityCpuIndex,
                                 task->taskCurPriority);
        /* @KEEP_COMMENT:
         * 将<task>任务插入对应优先级就绪队列ttosManager.priorityQueue尾部 */
        ttosReadyQueuePut (task, FALSE);
        /* @KEEP_COMMENT: 复位<task>的时间片大小 */
        task->tickSlice = task->tickSliceSize;
        /* @KEEP_COMMENT:
         * 重新获取最高优先级任务，保存于ttosManager.priorityQueueTask */
        ttosSetHighestPriorityTask (task->smpInfo.affinityCpuIndex);
#if defined(CONFIG_SMP)
    }
#endif
    /* 任务从非就绪变为就绪,检查是否需要对非当前CPU发送重调度IPI */
    (void)ttosTaskStateChanged (task, TRUE);
}
