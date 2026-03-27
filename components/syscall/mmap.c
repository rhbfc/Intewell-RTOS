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
#include <errno.h>
#include <sys/mman.h>
#include <ttosProcess.h>
#include <uaccess.h>

/**
 * @brief 系统调用实现：内存映射。
 *
 * 该函数实现了一个系统调用，用于将文件或设备映射到内存中。
 *
 * @param[in] addr 映射的起始地址（建议值）
 * @param[in] length 映射的长度
 * @param[in] prot 内存保护标志：
 *                 - PROT_NONE：不可访问
 *                 - PROT_READ：可读
 *                 - PROT_WRITE：可写
 *                 - PROT_EXEC：可执行
 * @param[in] flags 映射标志：
 *                  - MAP_SHARED：共享映射
 *                  - MAP_PRIVATE：私有映射
 *                  - MAP_FIXED：固定地址
 *                  - MAP_ANONYMOUS：匿名映射
 * @param[in] fd 文件描述符
 * @param[in] offset 文件偏移量
 * @return 成功时返回映射的内存地址，失败时返回MAP_FAILED。
 * @retval MAP_FAILED 映射失败。
 * @retval -EINVAL 参数无效。
 * @retval -ENOMEM 内存不足。
 * @retval -EACCES 权限不足。
 *
 * @note 1. addr通常为NULL让系统选择。
 *       2. length会对齐到页大小。
 *       3. prot必须兼容fd的访问模式。
 *       4. offset必须是页大小的整数倍。
 */
DEFINE_SYSCALL(mmap, (unsigned long addr, unsigned long length, unsigned long prot,
                      unsigned long flags, int fd, off_t offset))
{
    int ret;
    struct file *f = NULL;
    if (fd >= 0 && !(flags & MAP_ANONYMOUS))
    {
        ret = (ssize_t)fs_getfilep(fd, &f);

        if (ret < 0)
        {
            return ret;
        }
    }
    ret = process_mmap(ttosProcessSelf(), &addr, length, prot, flags, f, offset, length);
    if (ret < 0)
    {
        return ret;
    }
    return addr;
}
