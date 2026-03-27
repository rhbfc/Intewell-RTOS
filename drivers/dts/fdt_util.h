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

#ifndef __FDT_H__
#define __FDT_H__
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef isprint
#define in_range(c, lo, up) ((uint8_t)(c) >= lo && (uint8_t)c <= up)
#define isprint(c)          in_range ((c), 0x20, 0x7f)
#define isdigit(c)          in_range (c, '0', '9')
#define isxdigit(c)                                                            \
    (isdigit (c) || in_range (c, 'a', 'f') || in_range (c, 'A', 'F'))
#define islower(c) in_range (c, 'a', 'z')
#define isspace(c)                                                             \
    (c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v')
#define xchar(i) ((i) < 10 ? '0' + (i) : 'A' + (i)-10)
#endif

bool valid_header (char *p, size_t len);
int  fdt_dump (char *buf, size_t len);

#endif /* __FDT_H__ */