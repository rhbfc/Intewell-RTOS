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
#include <fs/fs.h>
#include <stdlib.h>
#include <ttosProcess.h>

/**
 * @brief 系统调用实现：删除目录。
 *
 * 该函数实现了一个系统调用，用于删除空目录。
 *
 * @param[in] pathname 要删除的目录路径
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功。
 * @retval -EACCES 权限不足。
 * @retval -ENOENT 目录不存在。
 * @retval -ENOTEMPTY 目录非空。
 *
 * @note 1. 只能删除空目录。
 *       2. 需要写权限。
 *       3. 不能删除当前目录。
 *       4. 不能删除根目录。
 */
DEFINE_SYSCALL (rmdir, (const char __user *pathname))
{
    int   ret;
    char *fullpath = process_getfullpath (AT_FDCWD, pathname);
    ret            = vfs_rmdir (fullpath);
    free (fullpath);

    if (ret < 0)
    {
        errno = -ret;
    }
    return ret;
}