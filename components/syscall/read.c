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
#include <malloc.h>
#include <ttos.h>

/**
 * @brief 系统调用实现：从文件描述符读取数据。
 *
 * 该函数实现了一个系统调用，用于从指定的文件描述符读取数据到用户空间缓冲区。
 * 读取操作会从文件的当前位置开始，读取成功后文件位置会相应地向后移动。
 *
 * @param[in] fd 文件描述符。
 * @param[out] buf 指向用户空间缓冲区的指针，用于存放读取的数据。
 * @param[in] count 要读取的字节数。
 * @return 成功时返回实际读取的字节数（可能小于请求的字节数），失败时返回负值错误码。
 * @retval >0 实际读取的字节数。
 * @retval 0  已到达文件末尾。
 * @retval -EBADF fd 不是有效的文件描述符或不是打开用于读取。
 * @retval -EFAULT buf 指向的地址超出进程的可访问地址空间。
 * @retval -EINTR 读取操作被信号中断。
 * @retval -EINVAL 无效参数。
 * @retval -EIO I/O 错误。
 */
DEFINE_SYSCALL (read, (int fd, char __user *buf, size_t count))
{
    ssize_t ret;
    if(count == 0)
    {
        return 0;
    }

    char   *kbuf = memalign(64, count);
    if (kbuf == NULL)
    {
        return -ENOMEM;
    }

    if (!user_access_check (buf, count, UACCESS_W))
    {
        free(kbuf);
        return -EINVAL;
    }
    if (fd == STDIN_FILENO)
    {
        struct kpollfd pfd;
        pfd.pollfd.fd     = fd;
        pfd.pollfd.events = POLLIN;

        ret = kpoll (&pfd, 1, TTOS_WAIT_FOREVER);
        if (ret < 0)
        {
            goto out;
        }
    }

    ret = vfs_read (fd, kbuf, count);

    if (ret > 0)
    {
        copy_to_user (buf, kbuf, ret);
    }
out:
    free (kbuf);
    return ret;
}
