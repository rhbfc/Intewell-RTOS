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
#include <process_signal.h>
#include <assert.h>
#include <uaccess.h>
#include <errno.h>

/**
 * @brief 系统调用实现：设置信号处理动作。
 *
 * 该函数实现了一个系统调用，用于设置指定信号的处理动作。
 *
 * @param[in] sig 信号编号
 * @param[in] act 新的信号处理动作结构
 * @param[out] oact 原信号处理动作结构
 * @param[in] sigsetsize 信号集大小
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功。
 * @retval -EINVAL 参数无效。
 * @retval -EFAULT 内存访问错误。
 * @retval -ENOSYS 不支持的信号。
 *
 * @note 1. 支持实时信号。
 *       2. 线程安全。
 *       3. 可恢复旧动作。
 *       4. 进程级设置。
 */
DEFINE_SYSCALL (rt_sigaction,
                     (int sig, const struct k_sigaction __user *act,
                      struct k_sigaction __user *oact, size_t sigsetsize))
{
    int ret;
    struct k_sigaction kernel_sigaction;
    struct k_sigaction kernel_sigaction_out;
    struct k_sigaction *kernel_sigaction_ptr     = NULL;
    struct k_sigaction *kernel_sigaction_out_ptr = NULL;

	if (sigsetsize != sizeof(k_sigset_t))
    {
        return -EINVAL;
    }

	if (act && copy_from_user(&kernel_sigaction, act, sizeof(kernel_sigaction)))
    {
        return -EFAULT;
    }

    kernel_sigaction_ptr     = act ? &kernel_sigaction : NULL;
    kernel_sigaction_out_ptr = oact ? &kernel_sigaction_out: NULL;

    ret = kernel_signal_action(sig, kernel_sigaction_ptr, kernel_sigaction_out_ptr);
	if (ret)
    {
		return ret;
    }

	if (oact && copy_to_user(oact, kernel_sigaction_out_ptr, sizeof(*kernel_sigaction_out_ptr)))
    {
		return -EFAULT;
    }
}
