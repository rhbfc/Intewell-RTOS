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

/**
 * @brief 系统调用实现：设置文件创建掩码。
 *
 * 该函数实现了一个系统调用，用于设置进程的文件创建掩码。
 * 掩码用于在创建文件时屏蔽某些权限位。
 *
 * @param[in] mask 新的掩码值，由以下权限位组成：
 *                - S_IRWXU (00700) 所有者权限掩码
 *                - S_IRWXG (00070) 组权限掩码
 *                - S_IRWXO (00007) 其他用户权限掩码
 * @return 返回之前的掩码值。
 *
 * @note 1. 掩码位为1表示对应权限被禁用。
 *       2. 掩码只影响后续创建的文件。
 *       3. 掩码是进程属性，子进程继承。
 *       4. 常用于限制文件权限。
 */
DEFINE_SYSCALL (umask, (int mask))
{
    return vfs_umask (mask);
}
