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

#ifndef __ARCH_AARCH64_UACCESS_H__
#define __ARCH_AARCH64_UACCESS_H__

#include <access_ok.h>

#ifdef CONFIG_SUPPORT_FAULT_RECOVERY

#define get_user(x, ptr)                                                                           \
    ({                                                                                             \
        const __typeof__(*(ptr)) __user *__gu_ptr = (ptr);                                        \
        __typeof__(*(ptr)) __gu_val = { 0 };                                                       \
        int __gu_ret;                                                                              \
        __chk_user_ptr(ptr);                                                                       \
        __gu_ret = arch_copy_from_user(&__gu_val, __gu_ptr, sizeof(*__gu_ptr));                   \
        if (__gu_ret)                                                                              \
        {                                                                                          \
            (x) = (__typeof__(x)){ 0 };                                                            \
        }                                                                                          \
        else                                                                                       \
        {                                                                                          \
            (x) = (__typeof__(x))__gu_val;                                                         \
        }                                                                                          \
        __gu_ret;                                                                                  \
    })

#define put_user(x, ptr)                                                                           \
    ({                                                                                             \
        __typeof__(*(ptr)) __pu_val = (__typeof__(*(ptr)))(x);                                     \
        __chk_user_ptr(ptr);                                                                       \
        arch_copy_to_user((ptr), &__pu_val, sizeof(__pu_val));                                    \
    })

#endif /* CONFIG_SUPPORT_FAULT_RECOVERY */

#endif /* __ARCH_AARCH64_UACCESS_H__ */
