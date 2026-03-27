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
#include <sysv/shm.h>

/**
 * @brief 系统调用实现：获取共享内存段。
 *
 * 该函数实现了一个系统调用，用于获取或创建一个共享内存段。
 *
 * @param[in] key 共享内存段的键值：
 *                - IPC_PRIVATE：创建新段
 *                - 其他值：用于标识现有或新建段
 * @param[in] size 共享内存段的大小（字节）
 * @param[in] flag 标志和权限：
 *                 - IPC_CREAT：不存在则创建
 *                 - IPC_EXCL：确保创建
 *                 - SHM_HUGETLB：使用大页
 *                 - 权限位（如0644）
 * @return 成功时返回共享内存段标识符，失败时返回负值错误码。
 * @retval -EACCES 权限不足。
 * @retval -EEXIST 段已存在（IPC_CREAT|IPC_EXCL）。
 * @retval -EINVAL 无效参数。
 * @retval -ENOMEM 内存不足。
 * @retval -ENOSPC 系统限制。
 *
 * @note 1. size会被页大小对齐。
 *       2. 新建段初始化为零。
 *       3. 段在最后一个引用释放后删除。
 *       4. 权限检查在创建和访问时进行。
 */
DEFINE_SYSCALL (shmget, (key_t key, size_t size, int flag))
{
    return sysv_shmget(key, size, flag);
}
