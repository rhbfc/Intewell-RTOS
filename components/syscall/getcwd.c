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
#include <assert.h>
#include <string.h>

/**
 * @brief 系统调用实现：获取当前工作目录。
 *
 * 该函数实现了一个系统调用，用于获取当前进程的工作目录的
 * 绝对路径。
 *
 * @param[out] buf 存储路径的缓冲区
 * @param[in] size 缓冲区大小
 * @return 成功时返回buf，失败时返回负值错误码。
 * @retval >0 成功，返回buf指针。
 * @retval -ERANGE 缓冲区太小。
 * @retval -EFAULT buf指向无效内存。
 * @retval -EINVAL size为0。
 * @retval -ENOENT 当前目录不存在。
 *
 * @note 1. 返回的路径总是以'/'开始。
 *       2. 路径包含终止的NULL字符。
 *       3. 如果buf为NULL且size不为0，会分配内存。
 *       4. 路径长度不会超过PATH_MAX。
 */
DEFINE_SYSCALL (getcwd, (char __user *buf, unsigned long size))
{
    pcb_t pcb = ttosProcessSelf ();
    assert (pcb != NULL);

    strncpy (buf, pcb->pwd, size);

    return strlen (pcb->pwd);
}