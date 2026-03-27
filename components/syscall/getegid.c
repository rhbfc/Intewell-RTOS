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
#include <fs/fs.h>
#include <ttosProcess.h>
#include <uaccess.h>
#include <unistd.h>

/**
 * @brief 系统调用实现：获取有效组ID。
 *
 * 该函数实现了一个系统调用，用于获取调用进程的有效组ID。
 *
 * @return 返回调用进程的有效组ID。
 * @retval >=0 成功，返回有效组ID。
 *
 * @note 1. 有效组ID用于文件访问权限检查。
 *       2. 可能与实际组ID不同。
 *       3. 通常由setegid设置。
 *       4. 用于权限检查时的身份验证。
 */
DEFINE_SYSCALL(getegid, (void))
{
    pcb_t pcb = ttosProcessSelf();
    if (pcb == NULL)
    {
        return -EINVAL;
    }
    return pcb->egid;
}

__alias_syscall(getegid, getegid32);