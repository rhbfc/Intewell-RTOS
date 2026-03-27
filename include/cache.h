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
 * @file： cache.h
 * @brief：
 *	    <li>CACHE相关操作</li>
 */
#ifndef _CACHE_H
#define _CACHE_H

/************************头文件********************************/
#ifndef ASM_USE

#include <stddef.h>

#endif

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/************************宏定义********************************/

/************************类型定义******************************/

/************************接口声明******************************/

#ifndef ASM_USE

void cache_icache_enable (void);
void cache_icache_disable (void);
void cache_dcache_enable (void);
void cache_dcache_disable (void);
void cache_icache_invalidate_all (void);
void cache_dcache_invalidate_all (void);
void cache_dcache_invalidate (size_t vaddr_start, size_t len);
void cache_dcache_clean_all (void);
void cache_dcache_clean (size_t vaddr_start, size_t len);
void cache_dcache_flush_all (void);
void cache_dcache_flush (size_t vaddr_start, size_t len);
void cache_text_update (size_t vaddr_start, size_t len);

#define invalidate_dcache_range(s,e) cache_dcache_invalidate(s, ((e)-(s)))
#define flush_dcache_range(s,e) cache_dcache_flush(s, ((e)-(s)))
#endif

#ifdef __cplusplus
}
#endif /*__cplusplus */

#endif /* _CACHE_H */
