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
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <ttosProcess.h>
#include <uaccess.h>

/**
 * @brief 系统调用实现：更改调用进程的根目录。
 *
 * @param[in] filename 指向文件夹路径的指针。
 *
 * @return 成功返回 0，失败返回负错误代码。
 * @retval -EACCES 路径前缀中的一个目录没有搜索权限。
 * @retval -EFAULT filename 指向了不可访问的地址空间。
 * @retval -EIO I/O 错误。
 * @retval -ELOOP 解析路径时遇到太多的符号链接。
 * @retval -ENAMETOOLONG 路径名太长。
 * @retval -ENOENT 文件不存在。
 * @retval -ENOMEM 内核内存不足。
 * @retval -ENOTDIR 路径前缀中的一个组件不是目录。
 * @retval -EPERM 调用进程没有足够的权限。
 * @retval -EROFS 文件位于只读文件系统上。
 */
DEFINE_SYSCALL (chroot, (const char __user *filename))
{
    int         ret;
    char       *fullpath;
    struct stat einfo;
    pcb_t       pcb;

    if (!user_string_access_ok (filename))
    {
        return -EFAULT;
    }

    fullpath = process_getfullpath (AT_FDCWD, filename);
    if (fullpath == NULL)
    {
        return -ENOENT;
    }

    ret = stat (fullpath, &einfo);
    if (ret < 0)
    {
        free (fullpath);
        return ret;
    }

    if (!S_ISDIR (einfo.st_mode))
    {
        free (fullpath);
        return -ENOTDIR;
    }

    pcb = ttosProcessSelf ();
    assert (pcb != NULL);
    strncpy (pcb->root, fullpath, sizeof (pcb->root));
    free (fullpath);

    return 0;
}
