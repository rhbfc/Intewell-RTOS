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
#include <ttosProcess.h>

/**
 * @brief 系统调用实现：获取父进程ID。
 *
 * 该函数实现了一个系统调用，用于获取调用进程的父进程ID。
 *
 * @return 返回调用进程的父进程ID。
 * @retval >0 成功，返回父进程ID。
 *
 * @note 1. 父进程ID是创建该进程的进程的ID。
 *       2. 如果父进程已终止，返回1（init进程）。
 *       3. init进程（PID=1）的PPID为0。
 *       4. 用于构建进程树关系。
 */
DEFINE_SYSCALL (getppid, (void))
{
    pcb_t pcb = ttosProcessSelf ();
    pcb_t parent = pcb->group_leader->parent;

    if (parent)
    {
        return get_process_pid (parent);
    }
    return 0;
}
