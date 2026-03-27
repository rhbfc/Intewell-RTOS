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
#include <errno.h>
#include <uaccess.h>
#include <unistd.h>

/**
 * @brief 系统调用实现：设置文件偏移量。
 *
 * 该函数实现了一个系统调用，用于设置文件的读写位置。
 *
 * @param[in] fd 文件描述符
 * @param[in] offset 偏移量
 * @param[in] whence 偏移起始位置：
 *                   - SEEK_SET：从文件开始处
 *                   - SEEK_CUR：从当前位置
 *                   - SEEK_END：从文件末尾
 * @return 成功时返回新的文件偏移量，失败时返回负值错误码。
 * @retval >=0 成功，返回新的文件偏移量。
 * @retval -EBADF fd不是有效的文件描述符。
 * @retval -EINVAL whence无效或结果位置无效。
 * @retval -ESPIPE fd引用管道、FIFO或套接字。
 *
 * @note 1. 偏移量可以超过文件末尾。
 *       2. 不能用于管道、FIFO和套接字。
 *       3. 写操作时可能扩展文件。
 *       4. 多进程共享文件偏移量。
 */
DEFINE_SYSCALL (lseek, (int fd, off_t offset, unsigned int whence))
{
    return vfs_lseek (fd, offset, whence);
}