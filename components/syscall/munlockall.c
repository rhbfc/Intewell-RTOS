/**
 * @file munlockall.c
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
 * @brief 系统调用实现：解除当前进程的所以锁定内存页到物理内存中，允许其被交换到硬盘（swap）
 *
 * 该函数实现了一个系统调用，解除当前进程的所以锁定内存页到物理内存中，允许其被交换到硬盘（swap）
 * 
 * 由于Intewell RTOS中不支持内存交换，因此该函数仅用于兼容性，不做任何实际操作。
 * @return 返回 0。
 */
DEFINE_SYSCALL (munlockall, (void))
{
    KLOG_I ("munlockall");
    return 0;
}
