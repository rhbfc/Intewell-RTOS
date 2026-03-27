/****************************************************************************
 * libs/libc/symtab/symtab_allsyms.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdlib.h>
#include <symtab.h>

/****************************************************************************
 * Public Data
 ****************************************************************************/
#if !defined(CONFIG_ALLSYMS)
const int g_nallsyms = 2;
const struct symtab_item g_allsyms[] =
{
    { "Unknown", (const  void *)0x00000000 },
    { "Unknown", (const  void *)0x00000000 },
};
#endif
/****************************************************************************
 * Name: allsyms_lookup
 *
 * Description:
 *   Find the symbol in the symbol table with the matching name.
 *
 * Returned Value:
 *   A reference to the symbol table entry if an entry with the matching
 *   name is found; NULL is returned if the entry is not found.
 *
 ****************************************************************************/

static const struct symtab_item *allsyms_lookup (const char *name, void *value,
                                                 size_t *size)
{
    const struct symtab_item *symbol = NULL;

    if (name)
    {
        symbol = symtab_findbyname (g_allsyms, name, g_nallsyms);
    }
    else if (value)
    {
        symbol = symtab_findbyvalue (g_allsyms, value, g_nallsyms);
    }

    if (symbol && symbol != &g_allsyms[g_nallsyms - 1])
    {
        *size = (symbol + 1)->sym_value - symbol->sym_value;
    }
    else
    {
        *size = 0;
    }

    return symbol;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: allsyms_findbyname
 *
 * Description:
 *   Find the symbol in the symbol table with the matching name.
 *
 * Returned Value:
 *   A reference to the symbol table entry if an entry with the matching
 *   name is found; NULL is returned if the entry is not found.
 *
 ****************************************************************************/

const struct symtab_item *allsyms_findbyname (const char *name, size_t *size)
{
    return allsyms_lookup (name, NULL, size);
}

/****************************************************************************
 * Name: symtab_findbyvalue
 *
 * Description:
 *   Find the symbol in the symbol table whose value closest (but not greater
 *   than), the provided value. This version assumes that table is not
 *   ordered with respect to symbol value and, hence, access time will be
 *   linear with respect to nsyms.
 *
 * Returned Value:
 *   A reference to the symbol table entry if an entry with the matching
 *   name is found; NULL is returned if the entry is not found.
 *
 ****************************************************************************/

const struct symtab_item *allsyms_findbyvalue (void *value, size_t *size)
{
    return allsyms_lookup (NULL, value, size);
}
