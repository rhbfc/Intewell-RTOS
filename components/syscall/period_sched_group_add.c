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

#include "syscall_internal.h"
#include <uaccess.h>
#include <period_sched_group.h>
#include <ttosProcess.h>

/**
 * @brief 系统调用实现：添加进程到调度组。
 *
 * 该函数实现了一个系统调用，用于将进程添加到调度组。
 *
 * @param tid 线程 ID。
 * @param sched_group_id 调度组 ID。
 * @return 成功时返回 0，失败时返回负值错误码。
 */
int syscall_period_sched_group_add(pid_t tid, int sched_group_id)
{
    pcb_t pcb;
    T_TTOS_TaskControlBlock *thread;

    thread = task_get_by_tid(tid);
    pcb = thread->ppcb;
	
    return period_sched_group_add (pcb, sched_group_id);
}