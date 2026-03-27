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
#include <ttos.h>
#include <uaccess.h>
#include <period_sched_group.h>

/**
 * @brief 系统调用实现：获取周期线程组中未启动的线程数量。
 *
 * 该函数实现了一个系统调用，用于查询当前周期线程组中尚未启动的线程数量。
 *
 * @return 返回未启动的线程数量。
 * @retval >=0 未启动的线程数量。
 * @retval -EINVAL 无效的周期线程组。
 *
 * @note 1. 仅统计当前周期线程组。
 *       2. 线程状态必须是未启动状态才会被计数。
 *       3. 线程组必须有效。
 */
int syscall_period_thread_get_unstart_num(void)
{
    return period_thread_get_unstart_num();
}