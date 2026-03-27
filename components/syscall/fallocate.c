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
 * @brief 系统调用实现：预分配文件空间。
 *
 * 该函数实现了一个系统调用，用于为文件预分配磁盘空间。
 * 可以防止写入时的空间不足问题。
 *
 * @param[in] fd 文件描述符
 * @param[in] mode 操作模式：
 *                - 0: 普通预分配
 *                - FALLOC_FL_KEEP_SIZE: 不修改文件大小
 *                - FALLOC_FL_PUNCH_HOLE: 创建空洞
 *                - FALLOC_FL_COLLAPSE_RANGE: 折叠范围
 *                - FALLOC_FL_ZERO_RANGE: 置零范围
 *                - FALLOC_FL_INSERT_RANGE: 插入范围
 * @param[in] offset 起始偏移量
 * @param[in] len 预分配长度
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 预分配成功。
 * @retval -EBADF fd不是有效的文件描述符。
 * @retval -EINVAL 无效参数。
 * @retval -EFBIG 文件太大。
 * @retval -ENOSPC 设备空间不足。
 * @retval -EPERM 文件系统不支持操作。
 *
 * @note 1. 预分配不会初始化文件内容。
 *       2. 不同文件系统支持的操作模式不同。
 *       3. 预分配可能改变文件大小。
 *       4. 某些操作可能需要文件系统支持。
 */
DEFINE_SYSCALL (fallocate, (int fd, int mode, loff_t offset, loff_t len))
{
    return vfs_ftruncate(fd, offset + len);
}