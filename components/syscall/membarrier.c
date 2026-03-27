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
#include <barrier.h>
#include <sys/membarrier.h>
#include <errno.h>

#define MEMBARRIER_CMD_QUERY                                0
#define MEMBARRIER_CMD_GLOBAL                               1
#define MEMBARRIER_CMD_GLOBAL_EXPEDITED                     2
#define MEMBARRIER_CMD_REGISTER_GLOBAL_EXPEDITED            4
#define MEMBARRIER_CMD_PRIVATE_EXPEDITED                    8
#define MEMBARRIER_CMD_REGISTER_PRIVATE_EXPEDITED           16
#define MEMBARRIER_CMD_PRIVATE_EXPEDITED_SYNC_CORE          32
#define MEMBARRIER_CMD_REGISTER_PRIVATE_EXPEDITED_SYNC_CORE 64
#define MEMBARRIER_CMD_PRIVATE_EXPEDITED_RSEQ               128
#define MEMBARRIER_CMD_REGISTER_PRIVATE_EXPEDITED_RSEQ      256

#define SUPPORT_CMD                                                            \
    (MEMBARRIER_CMD_GLOBAL | MEMBARRIER_CMD_GLOBAL_EXPEDITED                   \
     | MEMBARRIER_CMD_REGISTER_GLOBAL_EXPEDITED                                \
     | MEMBARRIER_CMD_PRIVATE_EXPEDITED                                        \
     | MEMBARRIER_CMD_REGISTER_PRIVATE_EXPEDITED                               \
     | MEMBARRIER_CMD_PRIVATE_EXPEDITED_SYNC_CORE                              \
     | MEMBARRIER_CMD_REGISTER_PRIVATE_EXPEDITED_SYNC_CORE                     \
     | MEMBARRIER_CMD_PRIVATE_EXPEDITED_RSEQ                                   \
     | MEMBARRIER_CMD_REGISTER_PRIVATE_EXPEDITED_RSEQ)


#undef SUPPORT_CMD
#define SUPPORT_CMD  0

/**
 * @brief 系统调用实现：内存屏障操作。
 *
 * 该函数实现了一个系统调用，用于执行全局内存屏障操作。
 *
 * @param[in] cmd 屏障命令：
 *                - MEMBARRIER_CMD_QUERY：查询支持的命令
 *                - MEMBARRIER_CMD_GLOBAL：全局内存屏障
 *                - MEMBARRIER_CMD_GLOBAL_EXPEDITED：快速全局内存屏障
 *                - MEMBARRIER_CMD_REGISTER_GLOBAL_EXPEDITED：注册快速全局屏障
 *                - MEMBARRIER_CMD_PRIVATE_EXPEDITED：私有快速内存屏障
 *                - MEMBARRIER_CMD_REGISTER_PRIVATE_EXPEDITED：注册私有快速屏障
 *                - MEMBARRIER_CMD_PRIVATE_EXPEDITED_SYNC_CORE：私有快速内存屏障（核心序列化）
 *                - MEMBARRIER_CMD_REGISTER_PRIVATE_EXPEDITED_SYNC_CORE：注册私有快速屏障（核心序列化）
 *                - MEMBARRIER_CMD_PRIVATE_EXPEDITED_RSEQ：私有快速内存屏障（rseq）
 *                - MEMBARRIER_CMD_REGISTER_PRIVATE_EXPEDITED_RSEQ：注册私有快速屏障（rseq）
 * @param[in] flags 命令标志
 * @return 成功时返回非负值，失败时返回负值错误码。
 * @retval >=0 成功，返回支持的命令掩码。
 * @retval -EINVAL cmd或flags无效。
 * @retval -EPERM 权限不足。
 * @retval -ENOMEM 内存不足。
 *
 * @note 1. 用于多处理器同步。
 *       2. 影响所有CPU核心。
 *       3. 可能导致性能开销。
 *       4. 需要特权权限。
 */
DEFINE_SYSCALL (membarrier, (int cmd, int flags))
{
    switch (cmd)
    {
    case MEMBARRIER_CMD_QUERY:
        return SUPPORT_CMD;
    case MEMBARRIER_CMD_GLOBAL:
    case MEMBARRIER_CMD_GLOBAL_EXPEDITED:
    case MEMBARRIER_CMD_REGISTER_GLOBAL_EXPEDITED:
    case MEMBARRIER_CMD_PRIVATE_EXPEDITED:
    case MEMBARRIER_CMD_REGISTER_PRIVATE_EXPEDITED:
    case MEMBARRIER_CMD_PRIVATE_EXPEDITED_SYNC_CORE:
    case MEMBARRIER_CMD_REGISTER_PRIVATE_EXPEDITED_SYNC_CORE:
    case MEMBARRIER_CMD_PRIVATE_EXPEDITED_RSEQ:
    case MEMBARRIER_CMD_REGISTER_PRIVATE_EXPEDITED_RSEQ:
        isb ();
        mb ();
        break;
    default:
        return -EINVAL;
    }
    return 0;
}
