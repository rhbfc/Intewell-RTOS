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
#include <sys/mount.h>
#include <ttosProcess.h>

/**
 * @brief 系统调用实现：卸载文件系统（带标志）。
 *
 * 该函数实现了一个系统调用，用于卸载文件系统，支持额外的标志选项。
 * 相比umount，提供了更多的控制选项。
 *
 * @param[in] name 要卸载的挂载点路径
 * @param[in] flags 卸载标志：
 *                  - MNT_FORCE：强制卸载
 *                  - MNT_DETACH：分离挂载
 *                  - MNT_EXPIRE：延迟卸载
 *                  - UMOUNT_NOFOLLOW：不跟随符号链接
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功卸载文件系统。
 * @retval -EBUSY 设备或资源正忙。
 * @retval -EINVAL 无效参数。
 * @retval -ENAMETOOLONG 路径名过长。
 * @retval -ENOENT 路径不存在。
 * @retval -EPERM 权限不足。
 *
 * @note 1. MNT_FORCE可能导致数据丢失。
 *       2. MNT_DETACH允许延迟卸载。
 *       3. 需要特权。
 *       4. 正在使用的文件系统可能无法卸载。
 */
DEFINE_SYSCALL (umount2, (char __user *name, int flags))
{
    int   ret      = 0;
    char *fullpath = NULL;

    fullpath = process_getfullpath (AT_FDCWD, name);
    switch (flags)
    {
    case MNT_FORCE:
    case MNT_DETACH:
    case MNT_EXPIRE:
    case UMOUNT_NOFOLLOW:
    default:
        ret = vfs_umount2 (fullpath, flags);
    }
    free (fullpath);

    return ret;
}