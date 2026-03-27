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
 * @file smc.S
 * @brief AArch64 SMC 调用封装。
 */

/************************头 文 件******************************/
#define ASM_USE
#include <asm.h>

/************************宏 定 义******************************/

/************************类型定义******************************/

/************************外部声明******************************/

/************************前向声明******************************/

/************************模块变量******************************/

/************************函数实现******************************/

/* 触发一次 SMC 调用，参数和返回值遵循 AArch64 调用约定。 */
ENTRY(smc_call)
    smc   #0
    ret
ENDPROC(smc_call)
