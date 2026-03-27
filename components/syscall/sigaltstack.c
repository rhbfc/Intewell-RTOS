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
#include <errno.h>
#include <uaccess.h>
#include <ttosProcess.h>

#if defined(__aarch64__)
#include <arch/aarch64/context.h>
#elif defined(__x86_64__)
#include <arch/x86_64/context.h>
#endif

/**
 * @brief 系统调用实现：设置和获取信号替代栈。
 *
 * 该函数实现了一个系统调用，用于设置和/或获取进程的信号替代栈。
 * 信号替代栈用于在原栈可能已损坏时处理信号。
 *
 * @param[in] uss 指向新的信号栈结构的指针：
 *                - ss_sp：栈的起始地址
 *                - ss_size：栈的大小
 *                - ss_flags：栈的标志
 * @param[out] uoss 用于返回旧的信号栈设置
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 操作成功。
 * @retval -EFAULT 内存访问错误。
 * @retval -EINVAL 无效参数。
 * @retval -ENOMEM 内存不足。
 * @retval -EPERM 权限不足。
 *
 * @note 1. 替代栈必须至少MINSIGSTKSZ字节。
 *       2. 不能在信号处理器中改变替代栈。
 *       3. 替代栈对每个线程独立。
 *       4. 替代栈状态通过ss_flags指示。
 */
DEFINE_SYSCALL(sigaltstack, (const struct sigaltstack __user *uss, struct sigaltstack __user *uoss))
{
    int ret;
    stack_t new, old;
    stack_t *new_ptr = NULL;
    stack_t *old_ptr = NULL;
    pcb_t pcb = ttosProcessSelf();

    if (uss && copy_from_user(&new, uss, sizeof(stack_t)))
    {
        return -EFAULT;
    }

    unsigned long sp = (unsigned long)exception_sp_get(&pcb->exception_context);

    if (uss)
    {
        new_ptr = &new;
    }

    if (uoss)
    {
        old_ptr = &old;
    }

    ret = kernel_sigaltstack(new_ptr, old_ptr, sp, MINSIGSTKSZ);

    if (!ret && uoss && copy_to_user(uoss, old_ptr, sizeof(stack_t)))
    {
        ret = -EFAULT;
    }

    return ret;
}

