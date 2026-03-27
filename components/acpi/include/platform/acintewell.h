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

#ifndef __ACINTEWELL_H__
#define __ACINTEWELL_H__

/* Common (in-kernel/user-space) ACPICA configuration */

#define ACPI_USE_SYSTEM_CLIBRARY
#define ACPI_USE_DO_WHILE_0
#define ACPI_IGNORE_PACKAGE_RESOLUTION_ERRORS
#define ACPI_SINGLE_THREADED
#define ACPI_USE_NATIVE_RSDP_POINTER

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ACPI_STRUCT_INIT(field, value) .field = value

#define ACPI_USE_STANDARD_HEADERS

#ifdef ACPI_USE_STANDARD_HEADERS
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#define ACPI_OFFSET(d, f) offsetof(d, f)
#endif

#if defined(__ia64__) || (defined(__x86_64__) && !defined(__ILP32__)) || defined(__aarch64__) ||   \
    defined(__PPC64__) || defined(__s390x__) || defined(__loongarch__) ||                          \
    (defined(__riscv) && (defined(__LP64__) || defined(_LP64)))
#define ACPI_MACHINE_WIDTH 64
#define COMPILER_DEPENDENT_INT64 long
#define COMPILER_DEPENDENT_UINT64 unsigned long

#define ACPI_USE_NATIVE_DIVIDE /* Has native 64-bit integer support */
#define ACPI_USE_NATIVE_MATH64 /* Has native 64-bit integer support */

#else
#define ACPI_MACHINE_WIDTH 32
#define COMPILER_DEPENDENT_INT64 long long
#define COMPILER_DEPENDENT_UINT64 unsigned long long
#define ACPI_USE_NATIVE_DIVIDE
#define ACPI_USE_NATIVE_MATH64
#endif

#ifndef __cdecl
#define __cdecl
#endif

#endif /* __ACINTEWELL_H__ */
