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
#include <sys/stat.h>

/**
 * @brief 系统调用实现：创建目录。
 *
 * 该函数实现了一个系统调用，用于创建一个新目录。
 *
 * @param[in] pathname 目录路径
 * @param[in] mode 目录权限模式
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功创建目录。
 * @retval -EACCES 没有写入权限。
 * @retval -EEXIST 目录已存在。
 * @retval -ENAMETOOLONG 路径名过长。
 * @retval -ENOENT 父目录不存在。
 *
 * @note 1. 需要父目录写权限。
 *       2. mode受umask影响。
 *       3. 创建"."和".."。
 *       4. 父目录链接数增加。
 */
DEFINE_SYSCALL (mkdir, (const char __user *pathname, mode_t mode))
{
    return SYSCALL_FUNC (mkdirat) (AT_FDCWD, pathname, mode);
}
