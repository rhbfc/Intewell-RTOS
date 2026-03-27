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
#include <uaccess.h>
#include <errno.h>

/**
 * @brief 系统调用实现：控制共享内存。
 *
 * 该函数实现了一个系统调用，用于对共享内存段执行各种控制操作。
 *
 * @param[in] shmid 共享内存段标识符
 * @param[in] cmd 控制命令：
 *                - IPC_STAT：获取shmid_ds结构
 *                - IPC_SET：设置shmid_ds结构
 *                - IPC_RMID：删除共享内存段
 *                - SHM_LOCK：锁定共享内存段
 *                - SHM_UNLOCK：解锁共享内存段
 * @param[in,out] buf 指向shmid_ds结构的指针：
 *                    - IPC_STAT时用于接收信息
 *                    - IPC_SET时用于提供新值
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 操作成功。
 * @retval -EINVAL 无效参数。
 * @retval -EFAULT buf指向无效内存。
 * @retval -EACCES 权限不足。
 * @retval -EPERM 操作不允许。
 *
 * @note 1. IPC_RMID会延迟到最后一个进程分离后。
 *       2. 需要适当的权限。
 *       3. buf的使用取决于cmd。
 *       4. 某些操作可能需要特权。
 */
DEFINE_SYSCALL (shmctl, (int shmid, int cmd, struct shmid_ds __user *buf))
{
    struct shmid_ds kds;
    if(copy_from_user(&kds, buf, sizeof(struct shmid_ds)) < 0)
    {
        return -EFAULT;
    }
    return sysv_shmctl(shmid, cmd, &kds);
}
