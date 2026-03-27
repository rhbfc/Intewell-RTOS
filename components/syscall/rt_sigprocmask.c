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
#include <process_signal.h>
#include <ttosProcess.h>
#include <uaccess.h>

#undef KLOG_TAG
#define KLOG_TAG "Signal"
#include <klog.h>

#undef KLOG_D
#define KLOG_D(...)

/**
 * @brief 系统调用实现：设置信号屏蔽字。
 *
 * 该函数实现了一个系统调用，用于设置或获取进程的信号屏蔽字。
 *
 * @param[in] how 操作类型：
 *                - SIG_BLOCK：添加信号到屏蔽字
 *                - SIG_UNBLOCK：从屏蔽字移除信号
 *                - SIG_SETMASK：设置新的屏蔽字
 * @param[in] set 新的信号集
 * @param[out] oldset 原信号集
 * @param[in] sigsetsize 信号集大小
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功。
 * @retval -EINVAL 参数无效。
 * @retval -EFAULT 内存访问错误。
 * @retval -ENOSYS 不支持的操作。
 *
 * @note 1. 支持实时信号。
 *       2. 线程安全。
 *       3. 进程级操作。
 *       4. 立即生效。
 */
DEFINE_SYSCALL(rt_sigprocmask,
               (int how, sigset_t __user *set, sigset_t __user *oset, size_t sigsetsize))
{
    int ret;
	k_sigset_t old_set;
    k_sigset_t new_set;
    pcb_t pcb = ttosProcessSelf();

	if (sigsetsize != sizeof(k_sigset_t))
    {
        return -EINVAL;
    }

    /* 记录当前信号mask */
	old_set = pcb->blocked;

	if (set)
    {
		if (copy_from_user(&new_set, set, sizeof(new_set)))
        {
			return -EFAULT;
        }

        signal_set_del_immutable(&new_set);

		ret = kernel_sigprocmask(how, &new_set, NULL);
		if (ret)
        {
			return ret;
        }
	}

    /* 将旧的mask复制到用户空间 */
	if (oset && copy_to_user(oset, &old_set, sigsetsize))
    {
		return -EFAULT;
	}

	return 0;
}