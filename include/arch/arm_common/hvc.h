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
 * @file： hvc.h
 * @brief：
 *	    <li>提供HVC接口声明。</li>
 */

#ifndef _HVC_H
#define _HVC_H

/************************头文件********************************/
#include <system/types.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/************************宏定义********************************/

/************************类型定义******************************/

/************************接口声明******************************/
s32 hvc_call (size_t arg0, size_t arg1, size_t arg2, size_t arg3);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _HVC_H */
