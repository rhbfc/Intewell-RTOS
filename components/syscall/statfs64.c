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
#include <uaccess.h>
#include <ttosProcess.h>

/**
 * @brief 系统调用实现：获取文件系统信息（64位版本）。
 *
 * 该函数实现了一个系统调用，用于获取指定路径所在文件系统的信息，使用64位数据结构。
 *
 * @param[in] path 要查询的文件系统的路径
 * @param[in] sz 缓冲区大小，必须大于等于statfs结构体的大小
 * @param[out] buf 用于存储文件系统信息的缓冲区
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功获取文件系统信息。
 * @retval -EINVAL 缓冲区大小不足。
 * @retval -EFAULT 内存访问错误。
 * @retval -ENOTDIR 路径组件不是目录。
 * @retval -ENOENT 路径不存在。
 *
 * @note 1. 支持大文件系统（>2GB）。
 *       2. 兼容32位系统。
 *       3. 缓冲区大小必须足够。
 *       4. 路径必须有效且可访问。
 */
DEFINE_SYSCALL (statfs64, (const char __user *path, size_t sz, struct statfs64 __user *buf))
{
    if(sz < sizeof(struct statfs))
    {
        return -EINVAL;
    }

    return SYSCALL_FUNC(statfs) (path, buf);
}