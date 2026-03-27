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
#include "ttosMM.h"

#include <assert.h>
#include <ttosProcess.h>

/**
 * @brief 系统调用实现：取消内存映射。
 *
 * 该函数实现了一个系统调用，用于取消之前建立的内存映射。
 *
 * @param[in] addr 要取消映射的内存区域起始地址
 * @param[in] length 要取消映射的内存区域长度
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功取消映射。
 * @retval -EINVAL 参数无效。
 * @retval -ENOMEM 内存不足。
 * @retval -EACCES 权限不足。
 *
 * @note 1. addr必须页对齐。
 *       2. length会向上对齐到页大小。
 *       3. 映射区域必须有效。
 *       4. 共享映射的更改会写回。
 */
DEFINE_SYSCALL (munmap, (unsigned long addr, size_t len))
{
    pcb_t pcb = ttosProcessSelf();
    struct mm *mm = get_process_mm(pcb);
    struct mm_region region;
    if(addr & (PAGE_SIZE - 1))
    {
        return -EINVAL;
    }

    mm_region_init(&region, 0, 0, len);
    region.virtual_address = addr;
    mm_region_modify(mm, &region, true);
    return 0;
}
