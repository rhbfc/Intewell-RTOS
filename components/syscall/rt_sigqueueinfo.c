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
#include <process_signal.h>
#include <uaccess.h>
#include <errno.h>

int kernel_sigqueueinfo (pid_t pid, int sig, siginfo_t *kinfo);

/**
 * @brief 系统调用实现：发送带数据的信号。
 *
 * 该函数实现了一个系统调用，用于向指定进程发送带有附加数据的信号。
 *
 * @param[in] pid 目标进程ID
 * @param[in] sig 信号编号
 * @param[in] uinfo 信号信息结构
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功。
 * @retval -EINVAL 参数无效。
 * @retval -EPERM 权限不足。
 * @retval -ESRCH 进程不存在。
 *
 * @note 1. 支持实时信号。
 *       2. 可携带数据。
 *       3. 进程间通信。
 *       4. 需要权限。
 */
DEFINE_SYSCALL (rt_sigqueueinfo, (pid_t pid, int sig, siginfo_t __user *uinfo))
{
    int ret;
    siginfo_t kinfo = {0};

    ret = copy_from_user(&kinfo, uinfo, sizeof(siginfo_t));
    if (ret)
    {
        return -EFAULT;
    }

    kinfo.si_signo = sig;

    return kernel_sigqueueinfo(pid, sig, &kinfo);
}
