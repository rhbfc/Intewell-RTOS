/**
 * @file mlockall.c
 * @author 张一弘 (zhangyihong@kyland.com)
 * @brief 
 * @version 3.0.0
 * @date 2024-11-22
 * 
 * @ingroup syscall
 * 
 * @copyright Copyright (c) 2024 Kyland Inc. All Rights Reserved.
 * 
 */
#include "syscall_internal.h"

/**
 * @brief 系统调用实现：锁定当前进程所有内存页到物理内存中，防止其被交换到硬盘（swap）
 *
 * 该函数实现了一个系统调用，锁定当前进程所有内存页到物理内存中，防止其被交换到硬盘（swap）
 * 
 * 由于Intewell RTOS中不支持内存交换，因此该函数仅用于兼容性，不做任何实际操作。
 * @param[in] flags 锁定标志，可选值如下：
 *             - MCL_CURRENT: 锁定当前进程的所有内存页。
 *             - MCL_FUTURE: 锁定当前进程的所有内存页，并对后续分配的内存页生效。
 * @return 返回 0。
 */
DEFINE_SYSCALL (mlockall, (int flags))
{
    KLOG_I ("mlockall flags: %d", flags);
    return 0;
}
