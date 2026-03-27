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

#include <ttosBase.h>

/**
 * @brief 系统调用实现：让当前线程让出处理器。
 *
 * 该函数实现了一个系统调用，使当前线程主动放弃处理器的使用权，
 * 让调度器有机会调度其他同优先级或更高优先级的线程运行。
 * 如果没有其他可运行的线程，当前线程会继续执行。
 *
 * @return 总是返回0。
 * @retval 0 操作成功。
 *
 * @note 调用此函数并不保证线程一定会让出处理器，这取决于系统的调度策略
 *       和其他可运行线程的存在。如果没有其他可运行的线程，调用线程会
 *       立即继续执行。
 */
DEFINE_SYSCALL (sched_yield, (void))
{
    TBSP_MSR_TYPE msr;
    ttosDisableTaskDispatchWithLock ();
    TBSP_GLOBALINT_DISABLE (msr);

    ttosRotateRunningTask (ttosGetRunningTask ());

    TBSP_GLOBALINT_ENABLE (msr);
    ttosEnableTaskDispatchWithLock ();

    return (0);
}