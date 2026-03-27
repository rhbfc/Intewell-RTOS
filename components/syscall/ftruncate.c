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

/**
 * @brief 系统调用实现：截断文件至指定长度。
 *
 * 该函数实现了一个系统调用，用于将文件截断或扩展到指定的长度。
 *
 * @param[in] fd 文件描述符
 * @param[in] length 目标长度
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 截断成功。
 * @retval -EBADF fd不是有效的文件描述符。
 * @retval -EINVAL fd不引用常规文件。
 * @retval -EPERM 文件不允许修改。
 * @retval -EROFS 文件系统只读。
 *
 * @note 1. 如果文件当前长度大于length，额外的数据将被丢弃。
 *       2. 如果文件当前长度小于length，文件将被用0字节填充。
 *       3. 修改文件长度需要写权限。
 *       4. 不能用于目录或特殊文件。
 */
DEFINE_SYSCALL (ftruncate, (int fd, loff_t length))
{
    return vfs_ftruncate(fd, length);
}

__alias_syscall(ftruncate, ftruncate64);