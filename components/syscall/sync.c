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
 * @brief 系统调用实现：同步文件系统缓冲区。
 *
 * 该函数实现了一个系统调用，用于将所有缓冲区写入磁盘。
 * 在当前实现中，由于系统特性，此功能暂时不需要。
 *
 * @return 总是返回0。
 *
 * @note 1. 此调用会阻塞直到所有数据都写入磁盘。
 *       2. 不保证数据的持久性，需要使用fsync确保。
 *       3. 在某些文件系统上可能会很慢。
 *       4. 不会报告错误。
 */
DEFINE_SYSCALL (sync, (void))
{
    /* 暂时不需要sync */
    return 0;
}
