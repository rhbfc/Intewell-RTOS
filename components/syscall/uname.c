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
#include <version.h>
#include <stdio.h>

/**
 * @brief 系统调用实现：获取系统信息。
 *
 * 该函数实现了一个系统调用，用于获取系统的标识信息，
 * 包括系统名称、版本、架构等。
 *
 * @param[out] name 用于存储系统信息的结构体：
 *                 - sysname：操作系统名称
 *                 - nodename：网络名称
 *                 - release：发行版本
 *                 - version：版本信息
 *                 - machine：硬件架构
 *                 - domainname：域名
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功获取系统信息。
 * @retval -EFAULT name指向无效内存。
 *
 * @note 1. 所有字符串都以NULL结尾。
 *       2. 字符串长度有限制。
 *       3. 信息不会动态更新。
 *       4. 用于系统识别。
 */
DEFINE_SYSCALL (uname, (struct utsname __user * name))
{
    snprintf (name->sysname, sizeof (name->sysname), "Intewell-RTOS");
    snprintf (name->machine, sizeof (name->machine), CONFIG_ARCH);
    snprintf (name->release, sizeof (name->release), INTEWELL_VERSION_STRING);
    snprintf (name->domainname, sizeof (name->domainname), "localhost");
    snprintf (name->nodename, sizeof (name->nodename), "Intewell-RTOS");
    snprintf (name->version, sizeof (name->version), "#%s %s", __DATE__,
              __TIME__);
    return 0;
}