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
 * @file： macros.h
 * @brief：
 *      <li>常用宏定义</li>
 */
#ifndef _MACROS_H
#define _MACROS_H

/************************头文件********************************/
#include <system/compiler.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/************************宏定义********************************/

#define tostring(s...)  #s
#define stringify(s...) tostring (s)

/* 最大值/最小值宏，进行了强制类型检测 */
#define min(x, y)                                                              \
    ({                                                                         \
        typeof (x) _min1 = (x);                                                \
        typeof (y) _min2 = (y);                                                \
        (void)(&_min1 == &_min2);                                              \
        _min1 < _min2 ? _min1 : _min2;                                         \
    })

#define max(x, y)                                                              \
    ({                                                                         \
        typeof (x) _max1 = (x);                                                \
        typeof (y) _max2 = (y);                                                \
        (void)(&_max1 == &_max2);                                              \
        _max1 > _max2 ? _max1 : _max2;                                         \
    })

#define sizeof_field(TYPE, MEMBER) sizeof ((((TYPE *)0)->MEMBER))
#define array_size(x)              (sizeof (x) / sizeof ((x)[0]))

/* 通过结构体的一个成员获取容器结构体的指针 */
#define container_of(ptr, type, member)                                        \
    ({                                                                         \
        const typeof (((type *)0)->member) *__mptr = (ptr);                    \
        (type *)((char *)__mptr - offsetof (type, member));                    \
    })

/* 对齐相关宏定义 */
#define __align_mask(x, mask) (((x) + (mask)) & ~(mask))
#define align(x, a)           __align_mask ((x), (__typeof__(x))(a) - 1)
#define align_mask(x, mask)   __align_mask ((x), (mask))
#define is_aligned(x, a)      (((x) & ((typeof (x))(a)-1)) == 0)

#define ALIGN_UP(x, align)   (((x) + ((typeof (x))(align) - 1)) & ~((typeof (x))(align) - 1))
#define ALIGN_DOWN(x, align) ((x) & ~((typeof (x))(align) - 1))

#define ROUNDUP(x, y)   ((x + (y - 1)) / (y)) * (y)
#define ROUNDDOWN(x, y) (((x) / (y)) * (y))
/************************类型定义******************************/

/************************接口声明******************************/

#ifdef __cplusplus
}
#endif /*__cplusplus */

#endif /*_MACROS_H */
