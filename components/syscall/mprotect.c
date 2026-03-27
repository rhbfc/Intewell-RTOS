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
#include <cache.h>
#include <errno.h>
#include <fs/fs.h>
#include <sys/mman.h>
#include <ttosMM.h>
#include <ttosProcess.h>
#include <uaccess.h>

#define PROT_NONE 0
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4
#define PROT_GROWSDOWN 0x01000000
#define PROT_GROWSUP 0x02000000

/**
 * @brief 系统调用实现：设置内存保护。
 *
 * 该函数实现了一个系统调用，用于修改内存区域的访问保护属性。
 *
 * @param[in] addr 内存区域起始地址
 * @param[in] len 内存区域长度
 * @param[in] prot 新的保护属性：
 *                 - PROT_NONE：不可访问
 *                 - PROT_READ：可读
 *                 - PROT_WRITE：可写
 *                 - PROT_EXEC：可执行
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功修改保护属性。
 * @retval -EINVAL 参数无效。
 * @retval -ENOMEM 内存不足。
 * @retval -EACCES 权限不足。
 *
 * @note 1. addr必须页对齐。
 *       2. len会向上对齐到页大小。
 *       3. 影响所有共享此内存的进程。
 *       4. 某些组合可能不被支持。
 */
DEFINE_SYSCALL(mprotect, (unsigned long addr, size_t len, unsigned long prot))
{
    struct mm_region region;
    int ret;
    pcb_t pcb = ttosProcessSelf();
    assert(pcb != NULL);
    mm_region_init(&region, 0, mm_v2p(get_process_mm(pcb), addr), len);
    region.virtual_address = addr;
    region.is_need_free = false;
    region.mem_attr = process_prot_to_attr(prot);

    ret = mm_region_modify(get_process_mm(pcb), &region, false);
    if (ret == 0)
    {
        if (!(MT_NO_ACCESS & region.mem_attr))
        {
            if (!(MT_EXECUTE_NEVER & region.mem_attr))
            {
                cache_text_update(region.virtual_address,
                                  region.region_page_count << PAGE_SIZE_SHIFT);
            }
        }
    }
    return ret;
}
