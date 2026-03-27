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
 * @brief 系统调用实现：同步文件数据和元数据。
 *
 * 该函数实现了一个系统调用，用于将文件的所有数据和元数据
 * 同步到存储设备。
 *
 * @param[in] fd 文件描述符
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 同步成功。
 * @retval -EBADF fd不是有效的文件描述符。
 * @retval -EINVAL fd引用不支持同步的对象。
 * @retval -EIO I/O错误。
 *
 * @note 1. 会等待所有数据写入完成。
 *       2. 包括文件数据和元数据。
 *       3. 比fdatasync更严格。
 *       4. 可能会影响性能。
 */
DEFINE_SYSCALL (fsync, (int fd))
{
    return vfs_fsync(fd);
}
