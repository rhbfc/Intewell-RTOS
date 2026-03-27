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

#ifdef CONFIG_EVENT_FD
#include "syscall_internal.h"
#include <fs/fs.h>
#include <errno.h>
#include <uaccess.h>
#include <sys/eventfd.h>

/**
 * @brief 系统调用实现：创建事件文件描述符（新版本）。
 *
 * 该函数实现了一个系统调用，用于创建一个事件通知文件描述符。
 * 是eventfd的新版本，提供更多标志选项。
 *
 * @param[in] count 计数器初始值
 * @param[in] flags 创建标志：
 *                - EFD_CLOEXEC: 设置close-on-exec标志
 *                - EFD_NONBLOCK: 设置非阻塞模式
 *                - EFD_SEMAPHORE: 信号量语义
 * @return 成功时返回新的事件文件描述符，失败时返回负值错误码。
 * @retval >0 新创建的事件文件描述符。
 * @retval -EINVAL flags参数无效。
 * @retval -EMFILE 达到进程打开文件描述符上限。
 * @retval -ENFILE 达到系统打开文件描述符上限。
 * @retval -ENOMEM 内存不足。
 *
 * @note 1. 读取返回计数器值，写入增加计数器值。
 *       2. EFD_SEMAPHORE使读取以原子方式递减计数器。
 *       3. 计数器为64位无符号整数。
 *       4. 推荐使用eventfd2代替eventfd。
 */
DEFINE_SYSCALL (eventfd2, (unsigned int count, int flags))
{
    return vfs_eventfd (count, flags);
}
#endif
