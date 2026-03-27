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
 * @brief 系统调用实现：获取实际组ID。
 *
 * 该函数实现了一个系统调用，用于获取调用进程的实际组ID。
 *
 * @return 返回调用进程的实际组ID。
 * @retval >=0 成功，返回实际组ID。
 *
 * @note 1. 实际组ID标识进程所属的组。
 *       2. 通常继承自父进程。
 *       3. 由setgid修改。
 *       4. 用于进程的组身份识别。
 */
DEFINE_SYSCALL(getgid, (void))
{
    pcb_t pcb = ttosProcessSelf();
    if (pcb == NULL)
    {
        return -EINVAL;
    }
    return pcb->gid;
}

__alias_syscall(getgid, getgid32);
