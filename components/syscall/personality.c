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
 * @brief 系统调用实现：设置进程执行域。
 *
 * 该函数实现了一个系统调用，用于设置进程的执行域特性。
 *
 * @param[in] personality 新的执行域特性：
 *                       - PER_LINUX：Linux执行域
 *                       - PER_BSD：BSD执行域
 *                       - PER_SVR4：SVR4执行域
 *                       - PER_MASK：特性掩码
 * @return 成功时返回旧的执行域值，失败时返回负值错误码。
 * @retval >=0 成功，返回旧值。
 * @retval -EINVAL 参数无效。
 *
 * @note 1. 影响进程行为。
 *       2. 主要用于兼容性。
 *       3. 可以组合特性。
 *       4. 子进程继承。
 */
DEFINE_SYSCALL (personality, (unsigned int personality))
{
    pcb_t pcb = ttosProcessSelf();
    if(personality != 0xffffffff)
    {
        pcb->group_leader->personality = personality;
    }
    return pcb->group_leader->personality;
}