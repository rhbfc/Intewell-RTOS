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
#include <assert.h>
#include <ttosBase.h>
#include <ttosProcess.h>

/**
 * @brief 系统调用实现：设置线程ID清除地址。
 *
 * 该函数实现了一个系统调用，用于设置线程退出时要清除的线程ID地址。
 *
 * @param[in] tidptr 指向要清除的线程ID的用户空间地址
 * @return 成功时返回当前线程ID，失败时返回负值错误码。
 * @retval >0 当前线程ID。
 * @retval -EFAULT 内存访问错误。
 *
 * @note 1. 线程退出时自动清除。
 *       2. 用于线程同步。
 *       3. 支持多线程。
 *       4. 用户空间地址。
 */
DEFINE_SYSCALL (set_tid_address, (int __user *tidptr))
{
    pcb_t pcb = ttosProcessSelf ();
    assert (pcb != NULL);
    pcb->clear_child_tid = tidptr;
    return (long)pcb->taskControlId->tid;
}