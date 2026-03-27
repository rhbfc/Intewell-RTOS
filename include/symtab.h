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

#ifndef __SYMTAB_H__
#define __SYMTAB_H__

#include <stddef.h>
#include <system/compiler.h>

struct symtab_item
{
    const char *sym_name;  /* A pointer to the symbol name string */
    const void *sym_value; /* The value associated with the string */
};
#if defined(CONFIG_ALLSYMS)
#define KSYM_EXPORT(symbol) __attribute__((used, section(".ksym_export"))) typeof(symbol) symbol

#define KSYM_EXPORT_ALIAS(old, symbol)                                                             \
    __attribute__((used, section(".ksym_export"))) typeof(old) old;                                \
    typeof(old) symbol __attribute__((__alias__(#old)))

#else
#define KSYM_EXPORT(...)
#endif

extern const struct symtab_item g_allsyms[];
extern const int g_nallsyms;
extern const struct symtab_item *symtab_findbyname(const struct symtab_item *symtab,
                                                   const char *name, int nsyms);
extern const struct symtab_item *symtab_findbyvalue(const struct symtab_item *symtab, void *value,
                                                    int nsyms);
extern const struct symtab_item *allsyms_findbyname(const char *name, size_t *size);
extern const struct symtab_item *allsyms_findbyvalue(void *value, size_t *size);
#endif
