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

/**
 * @brief 系统调用实现：通过文件描述符改变当前工作目录。
 *
 * 该函数实现了一个系统调用，用于将进程的当前工作目录改变为
 * 由文件描述符指定的目录。
 *
 * @param[in] fd 指向目标目录的文件描述符
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功改变工作目录。
 * @retval -EBADF fd不是有效的文件描述符。
 * @retval -ENOTDIR fd不是一个目录。
 * @retval -EACCES 没有搜索权限。
 *
 * @note 1. 进程必须对目录有执行(搜索)权限。
 *       2. 文件描述符必须引用一个目录。
 *       3. 改变工作目录只影响当前进程。
 *       4. 子进程会继承父进程的工作目录。
 */
DEFINE_SYSCALL (fchdir, (int fd))
{
    long ret;
    char *fullpath = process_getfullpath (fd, "");
    if(fullpath == NULL)
    {
        return -EBADF;
    }
    ret = SYSCALL_FUNC(chdir)(fullpath);
    free (fullpath);
    return ret;
}
