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
 *    禁止任务的可抢占。
 * @return:
 *    无
 * @notes:
 *    当禁止抢占之后，只要当前任务的状态为运行态，即使有更高优先级任务就绪也不进行任务切换。
 * @implements: RTE_DTASK.7.1
 */
void TTOS_DisablePreempt(void)
{
    T_TTOS_TaskControlBlock *tcb = ttosGetRunningTask();
    /* @REPLACE_BRACKET: TRUE == ttosIsISR()  */
    if (TRUE == ttosIsISR() || NULL == tcb)
    {
        return;
    }
    tcb->preemptCount++;
}
