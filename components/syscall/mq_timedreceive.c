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
#include <assert.h>
#include <time/ktime.h>
#include <errno.h>
#include <fs/fs.h>
#include <stdlib.h>
#include <ttosProcess.h>
#include <uaccess.h>

ssize_t vfs_mq_receive (mqd_t mqdes, char *msg, size_t msglen,
                        unsigned int *prio, T_UWORD wait_ticks);

/**
 * @brief 系统调用实现：从消息队列接收消息。
 *
 * 该函数实现了一个系统调用，用于从消息队列接收消息。
 *
 * @param mqdes 消息队列描述符。
 * @param msg_ptr 指向用户空间的消息缓冲区指针。
 * @param msg_len 消息缓冲区长度。
 * @param msg_prio 指向用户空间的消息优先级指针。
 * @param abs_timeout 指向用户空间的绝对超时时间结构体指针。
 * @return 成功时返回接收到的消息长度，失败时返回负值错误码。
 */
DEFINE_SYSCALL (mq_timedreceive, (mqd_t mqdes, char __user *msg_ptr,
                                  size_t msg_len, unsigned int __user *msg_prio,
                                  const struct timespec __user *abs_timeout))
{
    char           *kbuf;
    unsigned int    kprio;
    struct timespec ktime;
    long            ktime_value[2];
    ssize_t          ret = 0;
    ssize_t          len = 0;
    if (!user_access_check (msg_ptr, msg_len, UACCESS_W))
    {
        return -EINVAL;
    }

    if (msg_len == 0)
    {
        return -EINVAL;
    }

    kbuf = malloc (msg_len);
    if (kbuf == NULL)
    {
        return -ENOMEM;
    }

    if (abs_timeout)
    {
        ret = copy_from_user (ktime_value, abs_timeout, sizeof (ktime_value));
        if (ret < 0)
        {
            free (kbuf);
            return ret;
        }
        ktime.tv_sec  = ktime_value[0];
        ktime.tv_nsec = ktime_value[1];
        if (ktime.tv_sec < 0 || ktime.tv_nsec < 0 || ktime.tv_nsec >= NSEC_PER_SEC)
        {
            free (kbuf);
            return -EINVAL;
        }
        
    }

    len = vfs_mq_receive (mqdes, kbuf, msg_len, &kprio,
                          abs_timeout ? clock_time_to_tick (&ktime, true)
                                      : TTOS_WAIT_FOREVER);
    if (len > 0)
    {
        copy_to_user (msg_ptr, kbuf, min ((size_t)len, msg_len));
        if (msg_prio != NULL)
        {
            ret = copy_to_user (msg_prio, &kprio, sizeof (kprio));
            if (ret < 0)
            {
                free (kbuf);
                return ret;
            }
        }
        
    }

    free (kbuf);

    return len;
}
