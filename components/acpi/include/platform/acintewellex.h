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

#ifndef __ACINTEWELLEX_H__
#define __ACINTEWELLEX_H__

/*
 * Overrides for TTOS in-kernel ACPICA
 *
 * This file provides TTOS-specific implementations for ACPICA OS service layer.
 * It uses TTOS kernel memory management and synchronization primitives.
 */

#include <kmalloc.h>
#include <spinlock.h>

/* Memory allocation - use TTOS kernel allocators */
/* Tell ACPICA to use our native implementations */
#define USE_NATIVE_ALLOCATE_ZEROED
#define ACPI_USE_NATIVE_MEMORY_ALLOCATION

#endif /* __ACINTEWELLEX_H__ */
