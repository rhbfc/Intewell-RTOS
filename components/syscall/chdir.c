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
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <ttosProcess.h>
#include <uaccess.h>

/**
 * @brief 系统调用实现：改变当前工作目录。
 *
 * 该函数实现了一个系统调用，用于改变当前工作目录。
 *
 * @param[in] filename 目标目录名。
 * @return 成功时返回 0，失败时返回负值错误码。
 *
 * @retval -ENOENT 目录不存在。
 * @retval -EFAULT 参数指针超出用户空间。
 * @retval -EACCES 搜索权限被拒绝。
 * @retval -EIO I/O 错误。
 * @retval -ELOOP 符号链接解析过多。
 * @retval -ENAMETOOLONG 路径名太长。
 * @retval -ENOMEM 内核内存不足。
 * @retval -ENOTDIR 路径中的某个组件不是目录。
 *
 */
DEFINE_SYSCALL (chdir, (const char __user *filename))
{
    int         ret = -ENOENT;
    char       *fullpath;
    struct stat einfo;
    pcb_t       pcb;

    if (filename == NULL)
    {
        return -ENOENT;
    }

    if (!user_string_access_ok (filename))
    {
        return -EFAULT;
    }

    fullpath = process_getfullpath (AT_FDCWD, filename);
    if (fullpath == NULL)
    {
        return -ENOENT;
    }

    pcb = ttosProcessSelf ();
    assert (pcb != NULL);

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

    strncpy (pcb->pwd, fullpath, sizeof (pcb->pwd));
    free (fullpath);
    return 0;
}
