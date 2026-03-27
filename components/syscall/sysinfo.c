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
#include <malloc.h>
#include <page.h>
#include <ttosMM.h>
#include <ttosProcess.h>
#include <uaccess.h>

#ifdef CONFIG_SLUB
#include <slub.h>
#endif

/**
 * @brief 内核系统信息结构体
 */
struct kernel_sysinfo
{
    long uptime;             /**< 系统启动时间（秒） */
    unsigned long loads[3];  /**< 1、5、15分钟平均负载 */
    unsigned long totalram;  /**< 总内存大小 */
    unsigned long freeram;   /**< 空闲内存大小 */
    unsigned long sharedram; /**< 共享内存大小 */
    unsigned long bufferram; /**< 缓冲区大小 */
    unsigned long totalswap; /**< 总交换空间大小 */
    unsigned long freeswap;  /**< 空闲交换空间大小 */
    short procs;             /**< 进程数量 */
    short pad;               /**< 填充字段 */
    unsigned long totalhigh; /**< 高端内存大小 */
    unsigned long freehigh;  /**< 空闲高端内存大小 */
    unsigned mem_unit;       /**< 内存单位大小 */
};

/**
 * @brief 进程枚举回调函数
 *
 * 用于统计系统中的进程数量。
 *
 * @param[in] pcb 进程控制块
 * @param[in,out] data 用于存储进程计数的指针
 */
static void procfs_enum(pcb_t pcb, void *data)
{
    (*(short *)data)++;
}

/**
 * @brief 系统调用实现：获取系统信息。
 *
 * 该函数实现了一个系统调用，用于获取系统的各种统计信息，
 * 包括内存使用、进程数量、系统负载等。
 *
 * @param[out] info 用于返回系统信息的结构体指针
 * @return 成功时返回0，失败时返回负值错误码。
 * @retval 0 成功获取系统信息。
 * @retval -EINVAL info指向无效内存。
 *
 * @note 1. 内存单位为字节。
 *       2. 系统启动时间从启动开始计算。
 *       3. 某些字段可能始终为0。
 *       4. 进程数包括所有状态的进程。
 */
DEFINE_SYSCALL(sysinfo, (struct sysinfo __user * info))
{
    struct kernel_sysinfo *uinfo = (struct kernel_sysinfo *)info;
    struct timespec tp;
#ifndef CONFIG_SLUB
    struct memory_info *meminfo = get_malloc_info();
    uintptr_t page_nr, page_free;
    page_get_info(&page_nr, &page_free);
#else
    int total_pages, free_pages;
    slub_get_info(NULL, &total_pages, &free_pages);
#endif

    if (!user_access_check(uinfo, sizeof(struct kernel_sysinfo), UACCESS_W))
    {
        return -EINVAL;
    }
    clock_gettime(CLOCK_MONOTONIC, &tp);
    uinfo->uptime = tp.tv_sec;
    uinfo->loads[0] = 0;
    uinfo->loads[1] = 0;
    uinfo->loads[2] = 0;
    uinfo->mem_unit = 1; // B
    uinfo->totalswap = 0;
    uinfo->freeswap = 0;
    uinfo->procs = 0;
    process_foreach(procfs_enum, &uinfo->procs);
    uinfo->bufferram = 0;
    uinfo->freehigh = 0;
    uinfo->totalhigh = 0;
    uinfo->sharedram = 0;

#ifndef CONFIG_SLUB
    uinfo->freeram = meminfo->free + (page_free << PAGE_SIZE_SHIFT);
    uinfo->totalram = page_nr << PAGE_SIZE_SHIFT;
#else
    uinfo->freeram = (uint64_t)free_pages << PAGE_SIZE_SHIFT;
    uinfo->totalram = (uint64_t)total_pages << PAGE_SIZE_SHIFT;
#endif

    return 0;
}
