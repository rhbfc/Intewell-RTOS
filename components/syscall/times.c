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
#include <time/posix_timer.h>
#include <time/ktime.h>
#include <uaccess.h>
#include <errno.h>
#include <time.h>
#include <bits/alltypes.h>
#include <ttosProcess.h>

/**
 * @brief 系统调用实现：获取进程和子进程的CPU时间信息。
 *
 * 该函数实现了一个系统调用，用于获取当前进程和其子进程的CPU时间统计信息，
 * 包括用户态时间、系统态时间等。返回值是系统启动以来的时钟滴答数。
 *
 * @param[out] tbuf 时间信息结构体指针：
 *                - tms_utime: 进程在用户态的CPU时间
 *                - tms_stime: 进程在系统态的CPU时间
 *                - tms_cstime: 所有已终止子进程在系统态的CPU时间之和
 *                - tms_cutime: 所有已终止子进程在用户态的CPU时间之和
 * @return 成功时返回系统启动后的时钟滴答数，失败时返回-1。
 * @retval -1 tbuf指向无效内存。
 *
 * @note 时间单位是系统时钟滴答（clock tick），可以通过sysconf(_SC_CLK_TCK)
 *       获取每秒的时钟滴答数来转换为实际时间。
 */
DEFINE_SYSCALL(times, (struct tms __user *tbuf))
{
    pcb_t pcb = ttosProcessSelf();

    struct timespec tp;

    clock_gettime(CLOCK_MONOTONIC, &tp);

    tbuf->tms_utime = pcb->utime.tv_sec * MUSL_SC_CLK_TCK
                      + pcb->utime.tv_nsec / (NSEC_PER_SEC / MUSL_SC_CLK_TCK);
    tbuf->tms_stime = pcb->stime.tv_sec * MUSL_SC_CLK_TCK
                      + pcb->stime.tv_nsec / (NSEC_PER_SEC / MUSL_SC_CLK_TCK);
    tbuf->tms_cstime = tbuf->tms_stime;//未计算子进程
    tbuf->tms_cutime = tbuf->tms_utime;//未计算子进程

    return tp.tv_sec * MUSL_SC_CLK_TCK
                      + tp.tv_nsec / (NSEC_PER_SEC / MUSL_SC_CLK_TCK);
}
