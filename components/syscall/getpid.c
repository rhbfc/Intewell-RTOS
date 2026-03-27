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
#include <errno.h>
#include <assert.h>
#include <unistd.h>

/**
 * @brief 系统调用实现：获取进程ID。
 *
 * 该函数实现了一个系统调用，用于获取调用进程的进程ID。
 *
 * @return 返回调用进程的进程ID。
 * @retval >0 成功，返回进程ID。
 *
 * @note 1. 进程ID是唯一的进程标识符。
 *       2. 进程ID在进程创建时分配。
 *       3. 进程ID在进程终止后可能被重用。
 *       4. PID为1通常是init进程。
 */
DEFINE_SYSCALL (getpid, (void))
{
    pcb_t pcb = ttosProcessSelf ();
    assert (pcb != NULL);
    return get_process_pid (pcb);
}
