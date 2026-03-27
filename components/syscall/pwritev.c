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
#include <sys/uio.h>

/**
 * @brief 系统调用实现：向指定位置分散写入。
 *
 * 该函数实现了一个系统调用，用于从多个缓冲区向文件的指定位置写入数据。
 *
 * @param[in] fd 文件描述符
 * @param[in] vec iovec结构数组，每个结构包含：
 *                - iov_base：缓冲区地址
 *                - iov_len：缓冲区长度
 * @param[in] vlen 数组中的元素个数
 * @param[in] pos_l 文件中的起始位置（低32位）
 * @param[in] pos_h 文件中的起始位置（高32位）
 * @return 成功时返回写入的总字节数，失败时返回负值错误码。
 * @retval >0 成功写入的字节数。
 * @retval -EBADF 无效的文件描述符。
 * @retval -EINVAL 参数无效。
 * @retval -EFAULT 内存访问错误。
 *
 * @note 1. 不改变文件位置。
 *       2. 支持分散写入。
 *       3. 原子操作。
 *       4. 线程安全。
 */
DEFINE_SYSCALL (pwritev, (unsigned long fd, const struct iovec __user *vec,
                           unsigned long vlen, unsigned long pos_l,
                           unsigned long pos_h))
{
    return SYSCALL_FUNC (pwritev2) (fd, vec->iov_base, vec->iov_len, pos_l, pos_h, 0);
}
