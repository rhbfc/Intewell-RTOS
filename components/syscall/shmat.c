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
 * @brief 系统调用实现：附加共享内存段。
 *
 * 该函数实现了一个系统调用，用于将共享内存段附加到进程的地址空间。
 *
 * @param[in] shmid 共享内存段标识符
 * @param[in] shmaddr 指定的附加地址，如果为NULL则由系统选择
 * @param[in] shmflg 附加标志：
 *                   - SHM_RDONLY：只读方式附加
 *                   - SHM_RND：将地址向下舍入到SHMLBA边界
 *                   - SHM_REMAP：替换已有映射
 * @return 成功时返回共享内存段的起始地址，失败时返回MAP_FAILED。
 * @retval MAP_FAILED 附加失败。
 * @retval -EINVAL 无效参数。
 * @retval -ENOMEM 内存不足。
 * @retval -EACCES 权限不足。
 *
 * @note 1. 进程可以附加多个共享内存段。
 *       2. 地址对齐很重要。
 *       3. 子进程继承附加的段。
 *       4. 段在进程终止时自动分离。
 */
DEFINE_SYSCALL (shmat, (int shmid, char __user *shmaddr, int shmflg))
{
    return sysv_shmat(shmid, (virt_addr_t)shmaddr, shmflg);
}
