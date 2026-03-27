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

#include <ttosBase.h>
#include <ttosProcess.h>
#include <uaccess.h>
/**
 * @brief 系统调用实现：文件描述符多路复用。
 *
 * 该函数实现了一个系统调用，用于监视多个文件描述符的可读、可写和异常条件。
 *
 * @param[in] n 最大文件描述符加1。
 * @param[in,out] inp 要监视读就绪的文件描述符集。
 * @param[in,out] outp 要监视写就绪的文件描述符集。
 * @param[in,out] exp 要监视异常条件的文件描述符集。
 * @param[in] tsp 超时时间结构体（纳秒精度）：
 *                      - NULL: 永远等待
 *                      - {0,0}: 立即返回
 *                      - 其他: 等待指定时间
 * @return 成功时返回就绪描述符的数量，失败时返回负值错误码。
 * @retval >0 有事件发生的文件描述符数量。
 * @retval 0 超时。
 * @retval -EBADF 无效的文件描述符。
 * @retval -EINVAL n超过限制或tsp无效。
 * @retval -EFAULT 内存访问错误。
 * @retval -EINTR 被信号中断。
 * @retval -ENOMEM 内存不足。
 */

DEFINE_SYSCALL(select, (int n, fd_set __user *inp, fd_set __user *outp, fd_set __user *exp,
                        struct timeval __user *tvp))
{
    struct timespec *tsp = NULL;
    struct timespec ts;
    long val[2] = {0};

    if (tvp)
    {
        if (copy_from_user((void *)&val, (const void *)tvp, sizeof(val)))
        {
            return -EFAULT;
        }

        ts.tv_nsec = val[1] * 1000;
        ts.tv_sec = val[0];

        tsp = &ts;
    }

    return SYSCALL_FUNC(pselect6)(n, inp, outp, exp, tsp, NULL);
}