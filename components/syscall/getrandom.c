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
#include <fcntl.h>
#include <fs/fs.h>
#include <sys/random.h>

/**
 * @brief 系统调用实现：获取随机数。
 *
 * 该函数实现了一个系统调用，用于获取随机数。
 *
 * @param[out] buf 指向用户空间缓冲的指针，用于存放随机数。
 * @param[in] count 要获取的随机数字节数。
 * @param[in] flags 获取随机数的标志。
 *            - GRND_RANDOM 从随机源(/dev/random设备)提取随机字节
 *            - GRND_NONBLOCK 非阻塞的方式获取
 * @return 成功时返回读取的字节数，失败时返回负值错误码。
 * @retval -EAGAIN: 请求的熵不可用
 * @retval -EFAULT: buf指向的地址超出了可访问的地址空间
 * @retval -EINTR: 调用被信号处理程序中断
 * @retval -EINVAL: flags中指定了无效的标志
 * @retval -ENOSYS: 底层内核未实现此系统调用
 */
DEFINE_SYSCALL(getrandom, (char __user *buf, size_t count, unsigned int flags))
{
    const char *random_path;
    int ret;
    struct file f;

    if (flags == GRND_RANDOM)
    {
        random_path = "/dev/random";
    }
    else if (flags == GRND_NONBLOCK)
    {
        random_path = "/dev/urandom";
    }
    else if (flags == GRND_INSECURE)
    {
        random_path = "/dev/urandom";
    }
    /* 默认使用阻塞熵池 */
    else
    {
        random_path = "/dev/random";
    }
    ret = file_open(&f, random_path, O_RDONLY);
    if (ret < 0)
    {
        return -ENOENT;
    }
    ret = file_read(&f, buf, count);

    file_close(&f);

    return ret;
}