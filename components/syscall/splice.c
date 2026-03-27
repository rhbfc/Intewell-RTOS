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
#include <fs/kpoll.h>
#include <fs/fs.h>
#include <stdlib.h>
#include <ttos.h>
#include <uaccess.h>
#include <fcntl.h>
#include <restart.h>
#include <ttosProcess.h>

#define BUF_SIZE    4096

/**
 * @brief 系统调用实现：在两个文件描述符之间移动数据。
 *
 * 该函数实现了一个系统调用，用于在两个文件描述符之间直接传输数据，
 * 避免了数据在用户空间和内核空间之间的多次复制。
 *
 * @param[in] fd_in 输入文件描述符
 * @param[in,out] off_in 输入文件的偏移量，NULL表示使用当前偏移量
 * @param[in] fd_out 输出文件描述符
 * @param[in,out] off_out 输出文件的偏移量，NULL表示使用当前偏移量
 * @param[in] len 要传输的字节数
 * @param[in] flags 操作标志：
 *                 - SPLICE_F_MOVE：尝试直接移动页面（未实现）
 *                 - SPLICE_F_NONBLOCK：非阻塞操作
 *                 - SPLICE_F_MORE：更多数据将被追加（未实现）
 * @return 成功时返回传输的字节数，失败时返回负值错误码。
 * @retval >0 实际传输的字节数。
 * @retval -EAGAIN 非阻塞模式下无法立即完成操作。
 * @retval -EINVAL 参数无效。
 * @retval -EBADF 无效的文件描述符。
 * @retval -ERR_RESTART_IF_SIGNAL 操作被信号中断。
 * @retval -ENOMEM 内存不足。
 *
 * @note 1. 至少有一个文件描述符必须是管道。
 *       2. 如果指定了偏移量，文件位置不会改变。
 *       3. 非阻塞模式下可能传输的数据少于请求的数据。
 *       4. 支持被信号中断。
 */
DEFINE_SYSCALL (splice,
                 (int fd_in, loff_t __user *off_in, int fd_out,
                  loff_t __user *off_out, size_t len, unsigned int flags))
{

    int   ret;
    void *copy_buf;
    long  send_count = 0;

    if (off_in != NULL)
    {
        loff_t koffset;
        ret = copy_from_user (&koffset, off_in, sizeof (koffset));
        if (ret < 0)
        {
            return -EINVAL;
        }
            lseek (fd_in, koffset, SEEK_SET);
        }

    if(off_out != NULL)
    {
        loff_t koffset;
        ret = copy_from_user (&koffset, off_out, sizeof (koffset));
        if (ret < 0)
        {
            return -EINVAL;
        }
        lseek (fd_out, koffset, SEEK_SET);
    }

    copy_buf = memalign (sizeof (long), BUF_SIZE);

    struct file *in_file;
    fs_getfilep (fd_in, &in_file);
    if (NULL == in_file)
    {
        return -EBADF;
    }

    while (send_count < len)
    {
        ssize_t readlen;
        ssize_t writelen;
        ssize_t remain_rsize;
        remain_rsize = len - send_count;
        readlen = read (fd_in, copy_buf, (remain_rsize > BUF_SIZE) ? BUF_SIZE : remain_rsize);

        /* 读空了 */
        if (readlen == 0)
        {
            if(SPLICE_F_NONBLOCK & flags)
            {
                goto out;
            }
            else
            {
                struct kpollfd inpfd;
                inpfd.pollfd.fd     = fd_in;
                inpfd.pollfd.events = POLLIN;
                kpoll (&inpfd, 1, TTOS_WAIT_FOREVER);
                continue;
            }
        }
        /* 出错了 */
        if (readlen < 0)
        {
            if(readlen == -EAGAIN)
            {
                struct kpollfd inpfd;
                inpfd.pollfd.fd     = fd_in;
                inpfd.pollfd.events = POLLIN;
                kpoll (&inpfd, 1, TTOS_WAIT_FOREVER);
                continue;
            }
            /**
             * 在第一次socket读空readlen==0（对方连接关闭）时splice返回的为读取总长度，
             * 应用会认为还没读完，因此再触发一次splice，这时第二次读已经关闭的socket会得到-ENOTCONN，
             * 因此在这里返回0给应用正确通知已经读完
             */
            else if (readlen == -ENOTCONN && INODE_IS_SOCKET (in_file->f_inode))
            {
                readlen = 0;
            }
            send_count = readlen;
            goto out;
        }

        {
            // todo SPLICE_F_MORE
            size_t remain = readlen;
            while (remain)
            {
                struct kpollfd outpfd;
                outpfd.pollfd.fd     = fd_out;
                outpfd.pollfd.events = POLLOUT;
                int ret              = kpoll (&outpfd, 1, TTOS_WAIT_FOREVER);
                if (ret == 1 && (outpfd.pollfd.revents & POLLOUT))
                {
                    writelen
                        = write (fd_out, copy_buf + (readlen - remain), remain);
                    /* 修复：等于0时也应该退出 */
                    if (writelen <= 0)
                    {
                        if(writelen == -EAGAIN) continue;
                        send_count = writelen;
                        goto out;
                    }
                    remain -= writelen;
                }

                /* 可以被信号打断 */
                if(pcb_signal_pending(ttosProcessSelf()))
                {
                    send_count = -ERR_RESTART_IF_SIGNAL;
                    goto out;
                }
            }
            send_count += readlen;
        }
    }
out:
    free (copy_buf);
    return send_count;
}
