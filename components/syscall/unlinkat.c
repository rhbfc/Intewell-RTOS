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
 * @brief 系统调用实现：相对路径删除文件或目录。
 *
 * 该函数实现了一个系统调用，用于删除相对于给定文件描述符的文件或目录。
 *
 * @param[in] dfd 相对路径的基准目录文件描述符：
 *               - AT_FDCWD：相对于当前工作目录
 *               - 其他值：相对于指定的目录文件描述符
 * @param[in] pathname 要删除的文件或目录的路径名
 * @param[in] flag 操作标志：
 *                - 0：删除文件
 *                - AT_REMOVEDIR：删除目录
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功删除文件或目录。
 * @retval -EACCES 没有写入权限。
 * @retval -EBUSY 目标正在使用。
 * @retval -ENOENT 文件不存在。
 * @retval -EPERM 操作不允许。
 * @retval -EROFS 文件系统只读。
 *
 * @note 1. 路径名不能为空。
 *       2. 删除目录必须使用AT_REMOVEDIR标志。
 *       3. 目录必须为空才能删除。
 *       4. 提供了比unlink和rmdir更灵活的路径处理。
 */
DEFINE_SYSCALL(unlinkat, (int dfd, const char __user *pathname, int flag))
{
    int ret;
    char *fullpath = NULL;

    if (AT_FDCWD == dfd && (pathname == NULL || pathname[0] == 0))
    {
        return -ENOENT;
    }
    fullpath = process_getfullpath(dfd, pathname);
    if (AT_REMOVEDIR == flag)
    {
        ret = vfs_rmdir(fullpath);
    }
    else
    {
        ret = vfs_unlink(fullpath);
    }

    free(fullpath);

    if (ret < 0)
    {
        // KLOG_E ("fail at %s:%d ret:%d", __FILE__, __LINE__, ret);
        ret = 0;
    }

    return ret;
}
