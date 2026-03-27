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
#include <sys/mount.h>
#include <ttosProcess.h>

/**
 * @brief 系统调用实现：挂载文件系统。
 *
 * 该函数实现了一个系统调用，用于挂载文件系统。
 *
 * @param[in] source 源设备或文件系统
 * @param[in] target 挂载点路径
 * @param[in] filesystemtype 文件系统类型
 * @param[in] mountflags 挂载标志：
 *                       - MS_RDONLY：只读挂载
 *                       - MS_NOSUID：忽略suid和sgid位
 *                       - MS_NODEV：不允许访问设备文件
 *                       - MS_NOEXEC：不允许执行程序
 *                       - MS_SYNCHRONOUS：同步写入
 * @param[in] data 文件系统特定数据
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功挂载文件系统。
 * @retval -EACCES 没有权限。
 * @retval -EBUSY 目标已被挂载。
 * @retval -EINVAL 参数无效。
 * @retval -ENODEV 文件系统类型未知。
 *
 * @note 1. 需要特权权限。
 *       2. target必须是目录。
 *       3. source可以是设备或特殊文件系统。
 *       4. data格式依赖于文件系统类型。
 */
DEFINE_SYSCALL (mount, (char __user *dev_name, char __user *dir_name,
                  char __user *type, unsigned long flags, void __user *data))
{
    int   ret      = 0;
    char *fullpath = NULL;
    char kdev_name[NAME_MAX] = {0};
    char ktype[NAME_MAX] = {0};

    if(strlen_user(dev_name) > (sizeof(kdev_name) - 1))
    {
        return -ENAMETOOLONG;
    }
    if(strlen_user(type) > (sizeof(ktype) - 1))
    {
        return -ENAMETOOLONG;
    }

    ret = strcpy_from_user(kdev_name, dev_name);
    if(ret < 0)
    {
        return ret;
    }

    ret = strcpy_from_user(ktype, type);
    if(ret < 0)
    {
        return ret;
    }

    fullpath = process_getfullpath (AT_FDCWD, dir_name);
    ret = vfs_mount (dev_name, fullpath,
               ktype, flags,
               data);
    free (fullpath);

    return ret;
}