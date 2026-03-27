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
 * @file    ttosHandle.h
 * @brief
 *  <li>提供对象相关的宏定义和接口声明。</li>
 * @implements：
 */

#ifndef _TTOSHANDLEL_H
#define _TTOSHANDLEL_H

/************************头文件********************************/

#ifdef __cplusplus
extern "C"
{
#endif

/************************宏定义********************************/
#ifdef CONFIG_OS_LP64
#define HANDLE_MAGIC (0x10)
#define HANDLE_TYPE  (0x18)
#else
#define HANDLE_MAGIC (0x8)
#define HANDLE_TYPE  (0xc)
#endif

#define TTOS_VERIFY_OBJCORE_OK   U (0x510)
#define TTOS_VERIFY_OBJCORE_FAIL U (0x511)

/*检查对象的类型不匹配*/
#define TTOS_VERIFY_OBJCORE_TYPE_FAIL 0x512U

/************************类型定义******************************/
/************************接口声明******************************/
#ifndef ASM_USE
T_TTOS_ReturnCode ttosObjArchLibInit (T_VOID);
T_UWORD ttosVerifyObjectCore (T_TTOS_ObjectCoreID objectCoreID, T_UWORD type);
#endif

#ifdef __cplusplus
}
#endif

#endif
