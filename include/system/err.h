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

#ifndef __SYSTEM_ERR_H__
#define __SYSTEM_ERR_H__

#include <stdbool.h>
#include <stddef.h>
#include <system/compiler.h>

/*
 * 使用高地址保留区编码错误值，使接口可以通过同一个返回值传递
 * “有效指针”或“负错误码”。
 */
#define MAX_ERRNO 4095L

#ifndef __ASSEMBLY__

#define IS_ERR_VALUE(value) unlikely((unsigned long)(value) >= (unsigned long)(-MAX_ERRNO))

static inline void *__must_check ERR_PTR(long error)
{
    return (void *)error;
}

static inline long __must_check PTR_ERR(__force const void *ptr)
{
    return (long)ptr;
}

static inline bool __must_check IS_ERR(__force const void *ptr)
{
    return IS_ERR_VALUE((unsigned long)ptr);
}

static inline bool __must_check IS_ERR_OR_NULL(__force const void *ptr)
{
    return (ptr == NULL) || IS_ERR(ptr);
}

static inline void *__must_check ERR_CAST(__force const void *ptr)
{
    return (void *)ptr;
}

static inline int __must_check PTR_ERR_OR_ZERO(__force const void *ptr)
{
    return IS_ERR(ptr) ? (int)PTR_ERR(ptr) : 0;
}

#endif /* __ASSEMBLY__ */

#endif /* __SYSTEM_ERR_H__ */
