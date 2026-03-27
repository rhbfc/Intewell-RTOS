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
 * @file： const.h
 * @brief：
 *      <li>常量宏定义</li>
 */
#ifndef _CONST_H
#define _CONST_H

/************************头文件********************************/

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/************************宏定义********************************/

#ifdef ASM_USE
#define _AC(X, Y) X
#define _AT(T, X) X
#else
#define __AC(X, Y) (X##Y)
#define _AC(X, Y)  __AC (X, Y)
#define _AT(T, X)  ((T)(X))
#endif

#define _U(x)         (_AC (x, U))
#define _UL(x)        (_AC (x, UL))
#define _ULL(x)       (_AC (x, ULL))
#define U(x)          (_U (x))
#define UL(x)         (_UL (x))
#define ULL(x)        (_ULL (x))

#define BIT(nr)         (ULL(1) << (nr))

/************************类型定义******************************/

/************************接口声明******************************/

#ifdef __cplusplus
}
#endif /*__cplusplus */

#endif /*_CONST_H */
