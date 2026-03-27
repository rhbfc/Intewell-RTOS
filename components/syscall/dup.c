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
#include <fs/fs.h>

/**
 * @brief 系统调用实现：复制文件描述符。
 *
 * 该函数实现了一个系统调用，用于复制文件描述符。
 *
 * @param[in] fildes 文件描述符。
 * @return 成功时返回新的文件描述符，失败时返回负值错误码。
 * @retval -EBADF: 文件描述符非法
 * @retval -EINTR: 调用被信号中断
 * @retval -EMFILE: 文件描述符到达上限
 */
DEFINE_SYSCALL (dup, (unsigned int fildes))
{
    return vfs_dup(fildes);
}
