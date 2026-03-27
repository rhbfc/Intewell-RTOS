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
 * @brief 系统调用实现：从指定位置读取文件。
 *
 * 该函数实现了一个系统调用，用于从文件的指定位置读取数据。
 *
 * @param[in] fd 文件描述符
 * @param[out] buf 数据缓冲区
 * @param[in] count 要读取的字节数
 * @param[in] pos 文件中的起始位置
 * @return 成功时返回读取的字节数，失败时返回负值错误码。
 * @retval >0 成功读取的字节数。
 * @retval 0 到达文件末尾。
 * @retval -EBADF 无效的文件描述符。
 * @retval -EFAULT 内存访问错误。
 *
 * @note 1. 不改变文件位置。
 *       2. 线程安全操作。
 *       3. 支持大文件。
 *       4. 原子操作。
 */
DEFINE_SYSCALL (pread64, (int fd, char __user *buf, size_t count, loff_t pos))
{
    ssize_t ret;
    if(count == 0)
    {
        return 0;
    }

    char   *kbuf = malloc (count);
    if (kbuf == NULL)
    {
        return -ENOMEM;
    }

    if (!user_access_check (buf, count, UACCESS_W))
    {
        free(kbuf);
        return -EINVAL;
    }

    ret = vfs_pread (fd, kbuf, count, pos);

    if (ret > 0)
    {
        copy_to_user (buf, kbuf, ret);
    }

    free (kbuf);
    return ret;
}
