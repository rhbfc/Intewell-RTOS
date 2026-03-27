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

#include "sys/mman.h"
#include "syscall_internal.h"
#include <page.h>
#include <ttosMM.h>
#include <ttosProcess.h>

/**
 * @brief 系统调用实现：将数据段的结束地址设置为指定值。
 *
 * 该函数实现了一个系统调用，用于将数据段的结束地址设置为指定值。
 *
 * @param[in] brk: 新的数据段结束地址。
 * @return: 成功时返回 0，失败时返回负值错误码。
 * @retval: -ENOMEM 内存不足。
 * @retval: -EFAULT brk 指向的地址超出了进程的地址空间。
 * @retval: -EINVAL brk 指向的地址不是页对齐的。
 */
DEFINE_SYSCALL(brk, (unsigned long brk))
{
    pcb_t pcb = ttosProcessSelf();
    virt_addr_t old_brk;
    virt_addr_t new_brk;
    struct mm *mm = get_process_mm(pcb);

    if (brk == 0)
    {
        return mm->end_data_segment;
    }

    if (brk < mm->start_data_segment || brk > USER_SPACE_END)
    {
        return -EFAULT;
    }

    if (brk == mm->end_data_segment)
    {
        return brk;
    }

    old_brk = PAGE_ALIGN(mm->end_data_segment);
    new_brk = PAGE_ALIGN(brk);

    if (new_brk > old_brk)
    {
        for (virt_addr_t addr = old_brk; addr < new_brk; addr += PAGE_SIZE)
        {
            phys_addr_t phyaddr = process_mm_map(pcb, 0, &addr, MT_RW_DATA | MT_USER, 1,
                                                 MAP_FIXED | MAP_IS_HEAP | MAP_PRIVATE | MAP_ANON);
            if (phyaddr == 0)
            {
                return mm->end_data_segment;
            }
            /* 清空对应页 */
            memset((void *)(addr), 0, PAGE_SIZE);
        }
    }
    else
    {
        for (virt_addr_t addr = new_brk; addr < old_brk; addr += PAGE_SIZE)
        {
            process_mm_unmap(pcb, addr);
        }
    }

    mm->end_data_segment = brk;
    return brk;
}