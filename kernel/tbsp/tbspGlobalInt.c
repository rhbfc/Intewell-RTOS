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

/************************头 文 件******************************/

/* @<MOD_HEAD */
#include "ttosInterHal.h"
#include <ttos.h>
#include <ttos_arch.h>

/* @MOD_HEAD> */

/************************宏 定 义******************************/
/************************类型定义******************************/
/************************外部声明******************************/
/************************前向声明******************************/
/************************模块变量******************************/
/************************全局变量******************************/
/************************实    现******************************/
/**
 * @brief:
 *    禁止全局中断。
 * @return:
 *    中断屏蔽码。
 */
T_UWORD intCpuLock (T_VOID)
{
    TBSP_MSR_TYPE key = 0U;
    ttos_int_lock (key);
    return (key);
}

/**
 * @brief:
 *    恢复全局中断。
 * @param[in]: key: 中断屏蔽码
 * @return:
 *    无。
 */
T_VOID intCpuUnlock (T_ULONG key)
{
    ttos_int_unlock (key);
}
