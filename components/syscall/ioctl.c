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
#include <sys/ioctl.h>

/**
 * @brief 系统调用实现：设备控制操作。
 *
 * 该函数实现了一个系统调用，用于对设备文件进行控制操作。
 *
 * @param[in] fd 文件描述符
 * @param[in] cmd 控制命令
 * @param[in,out] arg 命令参数
 * @return 成功时返回0或正值，失败时返回负值错误码。
 * @retval >=0 成功，返回值依赖于具体命令。
 * @retval -EBADF fd不是有效的文件描述符。
 * @retval -ENOTTY fd不关联到字符设备。
 * @retval -EINVAL cmd或arg参数无效。
 * @retval -EFAULT arg指向无效内存。
 *
 * @note 1. 用于设备特定的控制操作。
 *       2. cmd的含义由设备驱动定义。
 *       3. arg的类型取决于cmd。
 *       4. 返回值的含义也由cmd定义。
 */
DEFINE_SYSCALL (ioctl, (int fd, unsigned int cmd, unsigned long arg))
{
    return vfs_ioctl (fd, cmd, arg);
}
