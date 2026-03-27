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
 * @file： elf.h
 * @brief：
 *	    <li> ELF文件相关的宏定义和类型定义  </li>
 */

#ifndef __ARCH_ARM64_ELF_H
#define __ARCH_ARM64_ELF_H

/************************头文件********************************/

#include <stdint.h>
#include <sys/user.h>

/************************宏定义********************************/

#define EM_ARCH EM_AARCH64

#define elf_check_arch(x) ((x)->e_machine == EM_AARCH64)

/* 通用寄存器个数，$x0--$x30 */
#define ELF_NGREG (31)

/************************类型定义******************************/

/************************接口声明******************************/

#endif /* __ARCH_ARM64_INCLUDE_ELF_H */
