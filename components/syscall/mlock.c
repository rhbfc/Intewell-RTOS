/**
 * @file mlock.c
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
 * @brief 系统调用实现：锁定内存页到物理内存中，防止其被交换到硬盘（swap）
 *
 * 该函数实现了一个系统调用，锁定内存页到物理内存中，防止其被交换到硬盘（swap）
 * 
 * 由于Intewell RTOS中不支持内存交换，因此该函数仅用于兼容性，不做任何实际操作。
 *
 * @param[in] start 要锁定的内存区域的起始地址。
 * @param[in] len 要锁定的内存区域的长度。
 * @return 返回 0。
 */
DEFINE_SYSCALL (mlock, (unsigned long start, size_t len))
{
    KLOG_I ("mlock start: 0x%lx, len: %zu", start, len);
    return 0;
}
