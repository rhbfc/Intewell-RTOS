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

#include <libfdt.h>

bool valid_header (char *p, size_t len)
{
    if (len < sizeof (struct fdt_header) || fdt_magic (p) != FDT_MAGIC
        || fdt_version (p) > FDT_LAST_SUPPORTED_VERSION
        || fdt_last_comp_version (p) > FDT_LAST_SUPPORTED_VERSION
        || fdt_totalsize (p) > len || fdt_off_dt_struct (p) >= len
        || fdt_off_dt_strings (p) >= len)
        return 0;
    else
        return 1;
}
