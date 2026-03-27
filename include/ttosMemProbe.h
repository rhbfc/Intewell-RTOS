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
 *@file:ttosMemProbe.h
 *@brief:
 *             <li>内存探测相关函数声明</li>
 */
#ifndef _TTOSMEMPROBELIB_H
#define _TTOSMEMPROBELIB_H

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/************************头 文 件******************************/
#include <ttosTypes.h>

/************************宏定义*******************************/
#define TTOS_READ_MEM  U (0)
#define TTOS_WRITE_MEM U (1)

/************************类型定义*****************************/
/************************接口声明*****************************/
T_TTOS_ReturnCode ttosMemProbeInit (T_VOID);
T_TTOS_ReturnCode ttosMemReadCheck (T_UWORD width, void *ptr);
T_TTOS_ReturnCode ttosMemWriteCheck (T_UWORD width, void *ptr, T_UWORD value);
T_TTOS_ReturnCode ttosMemRWCheck (T_UWORD width, void *ptr);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _TTOSMEMPROBELIB_H */
