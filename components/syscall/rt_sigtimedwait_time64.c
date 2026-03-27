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
#include <uaccess.h>
#include <process_signal.h>
#include <errno.h>

/**
 * @brief 系统调用实现：等待信号。
 *
 * 该函数实现了一个系统调用，用于等待信号。
 *
 * @param uthese 信号集指针。
 * @param uinfo 信号信息指针。
 * @param uts 超时时间指针。
 * @param sigsetsize 信号集大小。
 * @return 成功时返回 0，失败时返回负值错误码。
 */
DEFINE_SYSCALL (rt_sigtimedwait_time64, (const sigset_t __user *uthese, siginfo_t __user *uinfo,
                      const struct timespec __user *uts, size_t sigsetsize))
{
    k_sigset_t kset = {0};
    /* 用户态传入的uts为(long long)[2] */
    struct timespec64 kts64 = {0};

    if (sigsetsize != sizeof(k_sigset_t))
    {
        return -EINVAL;
    }

    if (copy_from_user(&kset, uthese, sizeof(kset)))
    {
        return -EFAULT;
    }

    if (uts && copy_from_user(&kts64, uts, sizeof(kts64)))
    {
        return -EFAULT;
    }

    return kernel_sigtimedwait(&kset, uinfo, uts ? &kts64 : NULL, sigsetsize);
}
