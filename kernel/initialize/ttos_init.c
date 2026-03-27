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

#include <ttos_init.h>

#define KLOG_TAG "INIT"
#include <klog.h>

#define INIT_FUNC_DEFINE(stage_name, index)                                    \
    static int stage_name##_init_start (void)                                  \
    {                                                                          \
        return 0;                                                              \
    }                                                                          \
    INIT_EXPORT_SECTION_START (stage_name##_init_start, index);                \
    static int stage_name##_init_end (void)                                    \
    {                                                                          \
        return 0;                                                              \
    }                                                                          \
    INIT_EXPORT_SECTION_END (stage_name##_init_end, index);                    \
    int ttos_##stage_name##_init (void)                                        \
    {                                                                          \
        int ret = 0;                                                           \
                                                                               \
        const struct init_data volatile *pos;                                  \
        for (pos = GET_INIT_TABLE (stage_name##_init_start) + 1;               \
             pos < GET_INIT_TABLE (stage_name##_init_end); pos++)              \
        {                                                                      \
            ret = pos->function ();                                            \
        }                                                                      \
        return ret;                                                            \
    }

INIT_FUNC_DEFINE (pre, INIT_STAGE_PRE)
INIT_FUNC_DEFINE (sys, INIT_STAGE_SYS)
INIT_FUNC_DEFINE (arch, INIT_STAGE_ARCH)
