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
#include "ttosInterHal.h"
#include "ttosUtils.h"
#include <ttosUtils.inl>

/**
 * @brief
 *       释放完成量
 * @param[in]   comp:   完成量控制块
 */
T_VOID TTOS_ReleaseCompletion(T_TTOS_CompletionControl *comp)
{

    if (comp == NULL)
    {
        return;
    }

    ttosDisableTaskDispatchWithLock();

    if (comp->flag == TTOS_COMPLETION_DONE)
    {
        ttosEnableTaskDispatchWithLock();
        return;
    }

    if (!list_is_empty(&(comp->waitQueue.queues.fifoQueue)))
    {
        ttosDequeueTaskq (&(comp->waitQueue));
    }
    else
    {
        comp->flag = TTOS_COMPLETION_DONE;
    }
    
    ttosEnableTaskDispatchWithLock();

    return;
}
