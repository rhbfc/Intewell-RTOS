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
#include <ttosProcess.h>

/**
 * @brief 系统调用实现：重新映射内存。
 *
 * 该函数实现了一个系统调用，用于扩展或缩小现有内存映射。
 *
 * @param[in] addr 原映射的起始地址
 * @param[in] old_len 原映射的大小
 * @param[in] new_len 新映射的大小
 * @param[in] flags 重映射标志：
 *                  - MREMAP_MAYMOVE：允许移动
 *                  - MREMAP_FIXED：使用指定地址
 * @param[in] new_addr 新的映射地址（仅用于MREMAP_FIXED）
 * @return 成功时返回新映射的地址，失败时返回MAP_FAILED。
 * @retval MAP_FAILED 重映射失败。
 * @retval -EINVAL 参数无效。
 * @retval -ENOMEM 内存不足。
 * @retval -EAGAIN 无法移动映射。
 *
 * @note 1. 地址必须页对齐。
 *       2. 大小会对齐到页大小。
 *       3. 可能会移动到新地址。
 *       4. 原内容会被保留。
 */
DEFINE_SYSCALL (mremap, (unsigned long addr, unsigned long old_len,
                         unsigned long new_len, unsigned long flags,
                         unsigned long new_addr))
{
    int ret = process_mremap (addr, old_len, new_len, flags, &new_addr);
    if(ret < 0)
    {
        return ret;
    }
    return new_addr;
}
