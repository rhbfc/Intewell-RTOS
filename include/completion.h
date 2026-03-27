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

#ifndef __COMPLETION_H__
#define __COMPLETION_H__

#include <ttos.h>

/* 定义完成量管理数据类型 */

#define TTOS_COMPLETION_WAIT    0
#define TTOS_COMPLETION_DONE    1

typedef struct {
    T_UWORD flag;
    T_TTOS_Task_Queue_Control waitQueue; 
} T_TTOS_CompletionControl;

T_TTOS_ReturnCode ttos_WaitCompletion(T_TTOS_CompletionControl *comp, T_UWORD ticks, T_BOOL is_interruptible);

T_TTOS_ReturnCode TTOS_InitCompletion(T_TTOS_CompletionControl *compCB);
T_TTOS_ReturnCode TTOS_WaitCompletion(T_TTOS_CompletionControl *comp, T_UWORD ticks);
T_VOID TTOS_ReleaseCompletion(T_TTOS_CompletionControl *comp);

#define TTOS_WaitCompletion(ttos_completion, ticks) ttos_WaitCompletion(ttos_completion, ticks, TRUE)
#define TTOS_WaitCompletionUninterruptible(ttos_completion, ticks) ttos_WaitCompletion(ttos_completion, ticks, FALSE)

#endif
