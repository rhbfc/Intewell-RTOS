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

/*
 * @file： syscalls.h
 * @brief：
 *	    <li>syscall相关函数声明及宏定义。</li>
 */
#ifndef _SYSCALLS_H
#define _SYSCALLS_H

/************************头文件********************************/
#include <syscall.h>
#include <system/types.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/************************宏定义********************************/
#define CONFIG_SYSCALL_NUM 447
#define CONFIG_EXTENT_SYSCALL_NUM_START 1000
#define CONFIG_EXTENT_SYSCALL_NUM_END   1009
#define CONFIG_EXTENT_SYSCALL_NUM (CONFIG_EXTENT_SYSCALL_NUM_END-CONFIG_EXTENT_SYSCALL_NUM_START+1)
/************************类型定义******************************/
typedef long (*syscall_func) (long, long, long, long, long, long);

/************************接口声明******************************/

int syscall_get_argc_count(long syscall_num);
const char *syscall_getname(long syscall_num);
bool is_extent_syscall_num (unsigned int syscall_num);
long syscall_dispatch (unsigned int syscall_num, unsigned long arg0, unsigned long arg1,
            unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* _SYSCALLS_H */
