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
 *    使能任务的可抢占。
 * @return:
 *    无
 * @notes:
 *    当使能抢占之后，其他高优先级任务可以抢占当前任务的CPU使用权限。
 * @implements: RTE_DTASK.43.1
 */
void TTOS_EnablePreempt (void)
{
    T_TTOS_TaskControlBlock *task = ttosGetRunningTask ();

    /* @REPLACE_BRACKET: TRUE == ttosIsISR()  */
    if (TRUE == ttosIsISR ())
    {
        return;
    }
    if (task == NULL)
    {
        return;
    }

    /* @REPLACE_BRACKET: U(0) != task->preemptCount  */
    if (0U != task->preemptCount)
    {
        /* @REPLACE_BRACKET: (--task->preemptCount) == U(0)  */
        task->preemptCount--;
        if ((task->preemptCount) == 0U)
        {
            /*进行任务调度 */
            (void)ttosSchedule ();
        }
    }
}
