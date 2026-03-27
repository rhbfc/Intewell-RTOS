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
#include <fs/kpoll.h>
#include <ttos.h>
#include <ttosProcess.h>
#include <uaccess.h>

/**
 * @brief 系统调用实现：多路复用 I/O 选择。
 *
 * 该函数实现了一个系统调用，用于多路复用 I/O 选择。
 *
 * @param[in] n 文件描述符集合中最大文件描述符值加 1。
 * @param[inout] inp 指向用户空间的读文件描述符集合指针。
 * @param[inout] outp 指向用户空间的写文件描述符集合指针。
 * @param[inout] exp 指向用户空间的异常文件描述符集合指针。
 * @param[in] tvp 指向用户空间的超时时间结构体指针。
 * @return 成功时返回就绪文件描述符的数量，失败时返回负值错误码。
 * @retval -EBADF  一个或多个文件描述符无效。
 * @retval -EINTR  被信号中断。
 * @retval -EINVAL 参数无效。
 * @retval -ENOMEM 内存不足。
 * 
 */
DEFINE_SYSCALL (_newselect, (int n, fd_set __user *inp, fd_set __user *outp,
                          fd_set __user *exp, struct timeval __user *tvp))
                          {
    int ret = 0, select_ret;

    fd_set kreadfds, kwritefds, kexceptfds;

    struct timeval timeout;

    long ktvp[2];

    if (inp)
    {
        ret = copy_from_user (&kreadfds, inp, sizeof (fd_set));
        if (ret < 0)
        {
            return ret;
        }
    }

    if (outp)
    {
        ret = copy_from_user (&kwritefds, outp, sizeof (fd_set));
        if (ret < 0)
        {
            return ret;
        }
    }

    if (exp)
    {
        ret = copy_from_user (&kexceptfds, exp, sizeof (fd_set));
        if (ret < 0)
        {
            return ret;
        }
    }

    if (tvp)
    {
        ret = copy_from_user (ktvp, tvp, sizeof (long) * 2);
        if (ret < 0)
        {
            return ret;
        }
        timeout.tv_sec  = ktvp[0];
        timeout.tv_usec = ktvp[1];
    }

    select_ret
        = vfs_select (n, inp ? &kreadfds : NULL, outp ? &kwritefds : NULL,
                      exp ? &kexceptfds : NULL, tvp ? &timeout : NULL);

    if (select_ret < 0)
    {
        return select_ret;
    }

    if (inp)
    {
        ret = copy_to_user (inp, &kreadfds, sizeof (fd_set));
        if (ret < 0)
        {
            return ret;
        }
    }

    if (outp)
    {
        ret = copy_to_user (outp, &kwritefds, sizeof (fd_set));
        if (ret < 0)
        {
            return ret;
        }
    }

    if (exp)
    {
        ret = copy_to_user (exp, &kexceptfds, sizeof (fd_set));
        if (ret < 0)
        {
            return ret;
        }
    }
    return select_ret;
}