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
#include <errno.h>
#include <uaccess.h>
#include <sys/wait.h>

/**
 * @brief 系统调用实现：终止进程组。
 *
 * 该函数实现了一个系统调用，用于终止当前线程组中的所有线程。
 * 所有线程的资源将被释放，状态码返回给父进程。
 *
 * @param[in] status 退出状态码：
 *                 - 0: 正常退出
 *                 - 非0: 错误退出
 * @return 从不返回。
 *
 * @note 1. 该调用永远不会返回到调用进程。
 *       2. 退出状态可通过wait/waitpid获取。
 *       3. 进程组的所有资源都会被释放。
 *       4. 线程组中的所有线程都会被终止。
 */
DEFINE_SYSCALL (exit_group, (int error_code))
{
    return process_exit_group(error_code, true);
}
