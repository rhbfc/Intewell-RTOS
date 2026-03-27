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
#include <string.h>

#define _GNU_SOURCE 1
#include <sys/mman.h>

#define SHM_PATH "/dev/shm/"
/**
 * @brief 系统调用实现：创建匿名文件。
 *
 * 该函数实现了一个系统调用，用于创建一个匿名文件。
 *
 * @param[in] uname_ptr 文件名（仅用于调试）
 * @param[in] flags 创建标志：
 *                  - MFD_CLOEXEC：执行时关闭
 *                  - MFD_ALLOW_SEALING：允许文件密封
 *                  - MFD_HUGETLB：使用大页内存
 * @return 成功时返回文件描述符，失败时返回负值错误码。
 * @retval >=0 成功，返回文件描述符。
 * @retval -EINVAL flags无效。
 * @retval -ENOMEM 内存不足。
 * @retval -EMFILE 打开文件过多。
 *
 * @note 1. 文件存在于内存中。
 *       2. 随进程终止自动删除。
 *       3. 可用于进程间通信。
 *       4. 支持所有文件操作。
 */
DEFINE_SYSCALL (memfd_create,
                (const char __user *uname_ptr, unsigned int flags))
{
    int   fd;
    char *name;
    int   ret;

    if (uname_ptr == NULL)
    {
        return -EINVAL;
    }

    name = malloc (strlen (uname_ptr) + strlen (SHM_PATH) + 1);
    if (name == NULL)
    {
        return -ENOMEM;
    }

    strcpy (name, SHM_PATH);

    ret = strcpy_from_user (name + sizeof (SHM_PATH) - 1, uname_ptr);
    if (ret < 0)
    {
        free (name);
        return -EFAULT;
    }

    int oflags = 0;

    if (MFD_CLOEXEC & flags)
    {
        oflags |= O_CLOEXEC;
    }

    // if(MFD_ALLOW_SEALING & flags)
    // {
    //     oflags |= O_ALLOW_SEALING;
    // }

    // if(MFD_HUGETLB & flags)
    // {
    //     oflags |= O_HUGETLB;
    // }

    fd = vfs_open (name, O_RDWR | O_CREAT | oflags, 0660);
    free (name);

    return fd;
}