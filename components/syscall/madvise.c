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
#include <fs/fs.h>
#include <errno.h>
#include <uaccess.h>
#include <sys/mman.h>

/**
 * @brief 系统调用实现：建议内存访问模式。
 *
 * 该函数实现了一个系统调用，用于向系统建议内存区域的访问模式。
 *
 * @param[in] addr 内存区域的起始地址
 * @param[in] length 内存区域的长度
 * @param[in] advice 访问建议：
 *                   - MADV_NORMAL：默认访问
 *                   - MADV_RANDOM：随机访问
 *                   - MADV_SEQUENTIAL：顺序访问
 *                   - MADV_WILLNEED：即将访问
 *                   - MADV_DONTNEED：暂不访问
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功设置访问建议。
 * @retval -EINVAL 参数无效。
 * @retval -ENOMEM 内存不足。
 * @retval -EBADF 地址无效。
 *
 * @note 1. 建议仅供参考。
 *       2. 系统可能忽略建议。
 *       3. 不影响内存语义。
 *       4. 用于性能优化。
 */
DEFINE_SYSCALL (madvise, (unsigned long addr, size_t length, int advice))
{
    /* 实时系统没有缺页异常 不需要考虑 */
    return 0;
}