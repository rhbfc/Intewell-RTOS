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

/**
 * @brief 系统调用实现：将文件描述符的读写位置设置为指定的偏移量。
 *
 * 该函数实现了一个系统调用，用于将文件描述符的读写位置设置为指定的偏移量。
 * 该函数的功能类似于 lseek 系统调用，但是支持 64 位的偏移量。
 *
 * @param[in] fd 文件描述符。
 * @param[in] offset_high 偏移量的高32位。
 * @param[in] offset_low 偏移量的低32位。
 * @param[out] result 指向用户空间的 loff_t 变量指针，用于返回新的读写位置。
 * @param[in] whence 偏移量的基准位置,可选值如下:
 *              - SEEK_SET: 从文件开始处偏移。
 *              - SEEK_CUR: 从当前读写位置偏移。
 *              - SEEK_END: 从文件末尾处偏移。
 * @return 成功时返回 0，失败时返回负值错误码。
 * @retval 0 成功。
 * @retval -EBADF  fd 并非一个已打开的文件描述符。
 * @retval -EFAULT 拷贝数据到用户空间失败。
 * @retval -EINVAL whence 参数无效。
 */
DEFINE_SYSCALL (_llseek, (int fd, unsigned long offset_high,
                          unsigned long offset_low, loff_t __user *result,
                          unsigned int whence))
{
    loff_t offset = ((off_t)offset_low) | (((off_t)offset_high) << 32);
    loff_t ret;

    if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END)
    {
        return -EINVAL;
    }

    ret = vfs_lseek (fd, offset, whence);

    if(ret < 0)
    {
        return ret;
    }
    if(result)
    {
        if(copy_to_user(result, &ret, sizeof(ret)))
        {
            return -EFAULT;
        }
    }

    return 0;
}