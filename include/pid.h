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

#ifndef __PID_H__
#define __PID_H__

#define __NEED_struct_timespec
#define __NEED_pid_t
#define __NEED_time_t

#ifdef _GNU_SOURCE
#define __NEED_size_t
#endif

#include <bits/alltypes.h>
#define PID_SETSIZE CONFIG_PROCESS_MAX

typedef struct pid_set_t
{
    unsigned long __bits[PID_SETSIZE / sizeof(long)];
} pid_set_t;

#define __PID_op_S(i, size, set, op)                                                               \
    ((i) / 8U >= (size) ? 0                                                                        \
                        : (((unsigned long *)(set))[(i) / 8 / sizeof(long)] op(                    \
                              1UL << ((i) % (8 * sizeof(long))))))

#define PID_SET_S(i, size, set) __PID_op_S(i, size, set, |=)
#define PID_CLR_S(i, size, set) __PID_op_S(i, size, set, &= ~)
#define PID_ISSET_S(i, size, set) __PID_op_S(i, size, set, &)

#define __PID_op_func_S(func, op)                                                                  \
    static __inline void __PID_##func##_S(size_t __size, pid_set_t *__dest,                        \
                                          const pid_set_t *__src1, const pid_set_t *__src2)        \
    {                                                                                              \
        size_t __i;                                                                                \
        for (__i = 0; __i < __size / sizeof(long); __i++)                                          \
            ((unsigned long *)__dest)[__i] =                                                       \
                ((unsigned long *)__src1)[__i] op((unsigned long *)__src2)[__i];                   \
    }

__PID_op_func_S(AND, &) __PID_op_func_S(OR, |) __PID_op_func_S(XOR, ^)

#define PID_AND_S(a, b, c, d) __PID_AND_S(a, b, c, d)
#define PID_OR_S(a, b, c, d) __PID_OR_S(a, b, c, d)
#define PID_XOR_S(a, b, c, d) __PID_XOR_S(a, b, c, d)

#define PID_ZERO_S(size, set) memset(set, 0, size)
#define PID_EQUAL_S(size, set1, set2) (!memcmp(set1, set2, size))

#define PID_SET(i, set) PID_SET_S(i, sizeof(pid_set_t), set)
#define PID_CLR(i, set) PID_CLR_S(i, sizeof(pid_set_t), set)
#define PID_ISSET(i, set) PID_ISSET_S(i, sizeof(pid_set_t), set)
#define PID_AND(d, s1, s2) PID_AND_S(sizeof(pid_set_t), d, s1, s2)
#define PID_OR(d, s1, s2) PID_OR_S(sizeof(pid_set_t), d, s1, s2)
#define PID_XOR(d, s1, s2) PID_XOR_S(sizeof(pid_set_t), d, s1, s2)
#define PID_ZERO(set) PID_ZERO_S(sizeof(pid_set_t), set)
#define PID_EQUAL(s1, s2) PID_EQUAL_S(sizeof(pid_set_t), s1, s2)

#endif /* __PID_H__ */
