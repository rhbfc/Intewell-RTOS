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
 * @file： ttos_time.h
 * @brief：
 *	    <li>time核心层相关定义。</li>
 */

#ifndef _TTOS_TIME_H
#define _TTOS_TIME_H
/************************头文件********************************/
#include <system/types.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/************************宏定义********************************/
#define NANOSECOND_PER_SECOND 1000000000UL

/************************类型定义******************************/

typedef struct ttos_time_ops {
    const char *time_name;
    s32 (*time_init) (void);
    s32 (*time_enable) (void);
    s32 (*time_disable) (void);
    u64 (*time_count_get) (void);
    s32 (*time_count_set) (u64 count);
    u64 (*time_freq_get) (void);
    u64 (*time_walltime_get) (void);
    s32 (*time_timeout_set) (u64 timeout);
    s32 (*time_timeout_ns_set) (u64 ns);
} ttos_time_ops_t;

typedef void (*time_handler_t) (void);

/************************外部声明******************************/

/* 物理timer使用 */
s32  ttos_time_register (ttos_time_ops_t *ops);
void ttos_time_unregister (void);
s32  ttos_time_init (void);
s32  ttos_time_disable (void);
s32  ttos_time_enable (void);
u64  ttos_time_count_get (void);
s32  ttos_time_count_set (u64 count);
u64  ttos_time_freq_get (void);
u64  ttos_time_walltime_get (void);
s32  ttos_time_timeout_set (u64 timeout);
s32  ttos_time_timeout_ns_set (u64 ns);
u64  ttos_time_count_to_ns (u64 count);
void ttos_time_ndelay (u64 ns);
void ttos_time_udelay (u64 us);
void ttos_time_mdelay (u64 ms);

/* 物理timer  中断注册与处理 */
s32  ttos_time_handler_register (time_handler_t handler);
void ttos_time_handler (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* _TTOS_TIME_H */
