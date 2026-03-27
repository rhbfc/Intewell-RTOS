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

/**
 * @file： cpu_bitops.S
 * @brief：
 *     <li>AArch64 CPU 位操作汇编辅助接口。</li>
 */

/************************头 文 件******************************/
#define ASM_USE
#include <asm.h>

.file	"cpu_bitops.S"

/************************宏 定 义******************************/

/************************类型定义******************************/

/************************外部声明******************************/

/************************前向声明******************************/

/************************模块变量******************************/

/************************全局变量******************************/

/************************实   现*******************************/
/*
 * @brief
 *    获取 32 位整数前导零的位数
 * @param[in] numVar: 32 位无符号整数
 * @retval 前导零位数；当参数为 0 时返回 32
 */
/*T_UWORD zero_num_of_word_get(T_UWORD numVar)*/
ENTRY(zero_num_of_word_get)
    clz w0, w0
    ret
ENDPROC(zero_num_of_word_get)
