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

#include "completion.h"
#include "ttosBase.h"

/*
 * @brief
 *       创建一个完成量
 * @return:
 * 完成量控制块指针
 */
T_TTOS_ReturnCode TTOS_InitCompletion(T_TTOS_CompletionControl *compCB)
{
    if (compCB == NULL)
    {
        return TTOS_INVALID_ID;
    }

    compCB->flag = TTOS_COMPLETION_WAIT;

    (void)ttosInitializeTaskq (&compCB->waitQueue, T_TTOS_QUEUE_DISCIPLINE_FIFO, TTOS_TASK_WAITING_COMPLETION);
    return TTOS_OK;
}
