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

/**
 * @brief 系统调用实现：卸载文件系统。
 *
 * 该函数实现了一个系统调用，用于卸载指定路径的文件系统。
 *
 * @param[in] name 要卸载的文件系统的路径名
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功卸载文件系统。
 * @retval -EPERM 权限不足。
 * @retval -EBUSY 文件系统正在使用。
 * @retval -EINVAL 无效的路径名。
 *
 * @note 1. 需要root权限。
 *       2. 文件系统必须已经挂载。
 *       3. 文件系统不能有打开的文件或活动进程。
 *       4. 该函数是umount2的简化版本，不支持额外的卸载标志。
 */
DEFINE_SYSCALL (umount, (char __user *name))
{
    return SYSCALL_FUNC (umount2) (name, 0);
}