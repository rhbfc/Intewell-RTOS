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
 * @brief 系统调用实现：分离共享内存段。
 *
 * 该函数实现了一个系统调用，用于将共享内存段从进程的地址空间分离。
 *
 * @param[in] shmaddr 要分离的共享内存段的地址
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功分离共享内存段。
 * @retval -EINVAL shmaddr不是有效的共享内存段地址。
 *
 * @note 1. 分离后内存段仍然存在。
 *       2. 只影响调用进程。
 *       3. 所有映射都会被移除。
 *       4. 如果是最后一个附加的进程且段被标记为删除，则段会被删除。
 */
DEFINE_SYSCALL (shmdt, (char __user *shmaddr))
{
    return sysv_shmdt((virt_addr_t)shmaddr);
}
