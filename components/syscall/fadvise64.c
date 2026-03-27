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

/**
 * @brief 系统调用实现：预告文件访问模式。
 *
 * 该函数实现了一个系统调用，用于向系统预告程序的文件访问模式。
 * 系统可以根据这些信息优化文件访问。
 *
 * @param[in] fd 文件描述符
 * @param[in] offset 文件偏移量
 * @param[in] len 区域长度
 * @param[in] advice 访问建议：
 *                 - POSIX_FADV_NORMAL: 默认访问
 *                 - POSIX_FADV_RANDOM: 随机访问
 *                 - POSIX_FADV_SEQUENTIAL: 顺序访问
 *                 - POSIX_FADV_WILLNEED: 即将需要
 *                 - POSIX_FADV_DONTNEED: 暂时不需要
 *                 - POSIX_FADV_NOREUSE: 一次性访问
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 建议已被接受。
 * @retval -EBADF fd不是有效的文件描述符。
 * @retval -EINVAL 无效参数。
 * @retval -ESPIPE fd指向管道或FIFO。
 *
 * @note 1. 这只是对系统的建议，系统可能不会采纳。
 *       2. 不同的文件系统可能有不同的支持程度。
 *       3. 建议应该基于实际的访问模式。
 *       4. 错误的建议可能导致性能下降。
 */
DEFINE_SYSCALL (fadvise64, (int fd, loff_t offset, size_t len, int advice))
{
    return 0;
}