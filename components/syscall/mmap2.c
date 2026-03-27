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
#include <fs/fs.h>
#include <sys/mman.h>
#include <ttosMM.h>
#include <ttosProcess.h>

/**
 * @brief 系统调用实现：将文件或设备映射到内存
 * 
 * 该函数实现了一个系统调用，用于将文件或设备映射到内存
 * 
 * @param[in] addr 映射地址
 * @param[in] len 映射长度
 * @param[in] prot 保护权限。它可以是PROT_NONE，或者以下一个或多个标志按位或：
 *              - PROT_EXEC 页面可以被执行
 *              - PROT_READ 页面可以被读取
 *              - PROT_WRITE 页面可以被写入
 *              - PROT_NONE 页面无法访问
 * @param[in] flags 映射标志，决定了对映射的更新是否对映射同一区域的其他进程可见，此行为通过标志中包含以下值之一确定：
 *              - MAP_SHARED 共享此映射，对映射的更新对映射同一区域的其他进程可见
 *              - MAP_SHARED_VALIDATE 与MAP_SHARED行为相同，但是MAP_SHARED_VALIDATE标志会检出所有传递的标志是否是正确，并对错误的标志返回错误
 *              - MAP_PRIVATE 创建一个私有的写实复制映射，对映射的更新对映射同一文件的其他进程不可见
 *              - MAP_32BIT 将映射放入进程地址空间的前2GB，此表示仅在x86-64上支持，适用于64位程序
 *              - MAP_ANON 为与其他实现的兼容性提供
 *              - MAP_ANONYMOUS 该映射不由任何文件系统，其内容初始化为零，fd参数被忽略，然而一些实现如果指定了MAP_ANONYMOUS，fd必须为-1，offset参数应为0
 *              - MAP_DENYWRITE 此标志被忽略
 *              - MAP_EXECUTABLE 此标志被忽略
 *              - MAP_FILE 兼容性标志，被忽略
 *              - MAP_FIXED addr必须适当的对其
 *              - MAP_FIXED_NOREPLACE 此标志提供了与addr强制执行相关的行为，类似于MAP_FIXED，不同之处在于MAP_FIXED_NOREPLACE永远不会覆盖已存在的映射范围
 *              - MAP_GROWSDOWN 该标志用于栈，向内核虚拟内存系统指示映射应向下扩展到内存中
 *              - MAP_HUGETLB 使用巨型页面分配映射
 *              - MAP_HUGE_2MB 与MAP_HUGETLB一起使用，页面大小为2MB
 *              - MAP_HUGE_1GB 与MAP_HUGETLB一起使用，页面大小为1GB
 *              - MAP_LOCKED 将区域映射标记为锁定
 *              - MAP_NONBLOCK 此标志仅在MAP_POPULATE一起使用时才有意义，不要进行预读，仅为已存在与RAM中的页面创建页表条目
 *              - MAP_NORESERVE 不为此映射保留交换空间
 *              - MAP_POPULATE 为映射填充页表，对于文件映射，会导致文件的预读
 *              - MAP_STACK 在适合进程或线程栈的地址分配映射
 *              - MAP_SYNC  此标志仅在MAP_SHARED_VALIDATE映射类型下可用
 *              - MAP_UNINITIALIZED 不清除匿名页面，此标志旨在提高嵌入式设备的性能
 * @param[in] fd 文件描述符
 * @param[in] pgoff 文件偏移
 * @return 成功时返回映射地址，失败时返回-1，并设置错误码
 * @retval -EFAULT: 从用户空间获取数据时出现问题
 * @retval -EINVAL: offset*4096不是系统页面的大小
 */
DEFINE_SYSCALL (mmap2, (unsigned long addr, unsigned long len, int prot,
                        int flags, int fd, long pgoff))
{
    return SYSCALL_FUNC (mmap) (addr, len, prot, flags, fd,
                                pgoff * ttosGetPageSize ());
}