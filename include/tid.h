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

#ifndef __TID_H__
#define __TID_H__

#include <unistd.h>

typedef struct T_TTOS_TaskControlBlock_Struct *TASK_ID;

void    tid_obj_free (pid_t tid);
pid_t   tid_obj_alloc (TASK_ID tcb);
pid_t   tid_obj_alloc_with_id (TASK_ID tcb, pid_t tid);
pid_t   get_max_tid (void);
void    task_foreach (void (*func) (TASK_ID, void *), void *arg);
TASK_ID task_get_by_tid (pid_t tid);

#endif /* __TID_H__ */