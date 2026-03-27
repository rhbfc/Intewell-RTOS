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
 * @file： cpuid.h
 * @brief：
 *	    <li>提供cpuid相关功能函数。</li>
 */

#ifndef _CPUID_H
#define _CPUID_H

/************************头文件********************************/
#include <stdbool.h>
#include <system/types.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/************************宏定义********************************/

/************************类型定义******************************/

/************************接口声明******************************/
void cpuid_set (void);
u32  cpuid_get (void);
bool is_bootcpu (void);
void bootcpu_init (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _CPUID_H */
