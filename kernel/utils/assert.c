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

#include <stdio.h>
#include <stdlib.h>
#include <ttos.h>
#include <commonUtils.h>

_Noreturn void __assert_fail (const char *expr, const char *file, int line,
                              const char *func)
{
    printk ("Assertion failed: %s (%s: %s: %d)\n", expr, file, func,
             line);
    while (1)
    {
        TTOS_SuspendTask (TTOS_SELF_OBJECT_ID);
    }
}
