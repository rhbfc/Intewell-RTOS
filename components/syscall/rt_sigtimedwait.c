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
#include <errno.h>
#include <process_signal.h>
#include <uaccess.h>

/**
 * @brief 系统调用实现：带超时等待信号。
 *
 * 该函数实现了一个系统调用，用于等待指定的信号集中的信号，可设置超时时间。
 *
 * @param[in] uset 等待的信号集
 * @param[out] uinfo 接收到的信号信息
 * @param[in] uts 超时时间结构
 * @param[in] sigsetsize 信号集大小
 * @return 成功时返回接收到的信号编号，失败时返回负值错误码。
 * @retval >0 接收到的信号编号。
 * @retval -EAGAIN 超时。
 * @retval -EINTR 被信号中断。
 * @retval -EINVAL 参数无效。
 *
 * @note 1. 支持实时信号。
 *       2. 可设置超时。
 *       3. 可获取信息。
 *       4. 可被中断。
 */
DEFINE_SYSCALL (rt_sigtimedwait, (const sigset_t __user *uthese, siginfo_t __user *uinfo,
                    const struct timespec __user *uts, size_t sigsetsize))
{
    k_sigset_t kset = {0};
    struct timespec64 kts64 = {0};
    struct timespec64 *kts64_ptr = NULL;

    if (sigsetsize != sizeof(k_sigset_t))
    {
        return -EINVAL;
    }

    if (copy_from_user(&kset, uthese, sizeof(kset)))
    {
        return -EFAULT;
    }

    if (uts)
    {
        if (copy_from_user(&kts64.tv_sec, &uts->tv_sec, sizeof(uts->tv_sec)))
        {
            return -EFAULT;
        }

        if (copy_from_user(&kts64.tv_nsec, &uts->tv_nsec, sizeof(uts->tv_nsec)))
        {
            return -EFAULT;
        }

        kts64_ptr = &kts64;
    }

    return kernel_sigtimedwait(&kset, uinfo, kts64_ptr, sigsetsize);
}
