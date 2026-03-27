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
#include <ttos.h>
#include <ttosInterTask.inl>

/**
 * @brief 系统调用实现：获取当前线程ID。
 *
 * 该函数实现了一个系统调用，用于获取当前线程ID。
 *
 * @return 当前线程ID。
 */
DEFINE_SYSCALL (gettid, (void))
{
    return (long)(ttosGetRunningTask ()->taskControlId->tid);
}