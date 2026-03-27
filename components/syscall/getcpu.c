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
#include <cpuid.h>

/**
 * @brief 系统调用实现：获取当前CPU和NUMA节点信息。
 *
 * 该函数实现了一个系统调用，用于获取当前进程运行的CPU编号
 * 和NUMA节点编号。
 *
 * @param[out] cpu 存储CPU编号的指针
 * @param[out] node 存储NUMA节点编号的指针
 * @param[in] tcache 未使用的参数
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 获取成功。
 * @retval -EFAULT 指针指向无效内存。
 *
 * @note 1. cpu和node参数可以为NULL。
 *       2. tcache参数已废弃，应设为NULL。
 *       3. 返回的是调用时刻的信息。
 *       4. 进程可能随后被调度到其他CPU。
 */
DEFINE_SYSCALL (getcpu, (unsigned __user *cpu, unsigned __user *node,
                          void __user *tcache))
{
    unsigned int kcpu;
    int ret;

    kcpu = cpuid_get();
    ret = copy_to_user(cpu, &kcpu, sizeof(int));
    if (ret)
    {
        return -EFAULT;
    }

    return 0;
}
