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
#include <unistd.h>

/**
 * @brief 系统调用实现：执行程序。
 *
 * 该函数实现了一个系统调用，用于执行新的程序。
 * 新程序将替换当前进程的文本、数据和堆栈。
 *
 * @param[in] filename 要执行的程序文件路径
 * @param[in] argv 命令行参数数组，以NULL结尾
 * @param[in] envp 环境变量数组，以NULL结尾
 * @return 成功时从不返回，失败时返回负值错误码。
 * @retval -EACCES 无执行权限。
 * @retval -ENOENT 文件不存在。
 * @retval -ENOTDIR 路径组件不是目录。
 * @retval -ENOMEM 内存不足。
 * @retval -E2BIG 参数或环境太大。
 * @retval -ETXTBSY 可执行文件正在被写入。
 * @retval -ENOEXEC 执行格式错误。
 *
 * @note 1. 调用成功时不返回，失败时返回到调用进程。
 *       2. 新程序继承父进程的大部分属性。
 *       3. 文件描述符的关闭行为由FD_CLOEXEC标志控制。
 *       4. 信号处理被重置为默认行为。
 */
DEFINE_SYSCALL (execve, (const char __user               *filename,
                         const char __user *const __user *argv,
                         const char __user *const __user *envp))
{
    return do_execve (filename, argv, envp);
}