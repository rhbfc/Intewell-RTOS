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

#ifndef __ACCESS_OK_H__
#define __ACCESS_OK_H__

#include "mmu.h"
#include <system/compiler.h>

static inline int __access_ok(const void *ptr, unsigned long size)
{
	unsigned long start = USER_SPACE_START;
	unsigned long limit = USER_SPACE_END - USER_SPACE_START + 1;
	unsigned long addr = (unsigned long)ptr;

	return (size <= limit) && (addr >= start) && ((addr - start) <= (limit - size));
}

#define access_ok(addr, size) likely(__access_ok(addr, size))

#endif /* __ACCESS_OK_H__ */
