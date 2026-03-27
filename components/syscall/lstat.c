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
#include <fs/kstat.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <ttosProcess.h>
#include <uaccess.h>

/**
 * @brief 系统调用实现：获取符号链接状态。
 *
 * 该函数实现了一个系统调用，用于获取符号链接本身的状态信息。
 *
 * @param[in] filename 符号链接的路径
 * @param[out] statbuf 用于存储状态信息的缓冲区
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功获取状态信息。
 * @retval -EACCES 没有搜索权限。
 * @retval -EFAULT statbuf指向无效内存。
 * @retval -ENAMETOOLONG 路径名过长。
 * @retval -ENOENT 路径不存在。
 *
 * @note 1. 不会跟随符号链接。
 *       2. 使用64位文件大小。
 *       3. 返回链接本身的信息。
 *       4. 需要搜索目录的权限。
 */

DEFINE_SYSCALL(lstat, (const char __user *filename, struct stat __user *statbuf))
{
    return SYSCALL_FUNC(lstat64)(filename, statbuf);
}