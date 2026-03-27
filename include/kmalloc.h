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

#ifndef __KMALLOC_H__
#define __KMALLOC_H__

#include <malloc.h>
#include <stddef.h>
#include <system/gfp_types.h>

void *dma_malloc(size_t size);
void *dma_memalign(size_t align, size_t len);
void *zalloc(size_t size);

void *kmalloc(size_t size, gfp_t flags);
void kfree(void *ptr);
size_t ksize(void *ptr);
void *kzalloc(size_t size, gfp_t flags);
void *krealloc(void *ptr, size_t size, gfp_t flags);

#endif /* __KMALLOC_H__ */
