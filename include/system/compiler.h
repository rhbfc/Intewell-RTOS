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
 * @file： compiler.h
 * @brief：
 *      <li>GCC编译扩展属性</li>
 */
#ifndef _COMPILER_H
#define _COMPILER_H

/************************头文件********************************/

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/************************宏定义********************************/
#define barrier() __asm__ __volatile__("" : : : "memory")
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#define __noinline __attribute__((noinline))
#define __unused __attribute__((unused))
#define __used __attribute__((used))
#define __aligned(x) __attribute__((aligned(x)))
#define __noreturn __attribute__((noreturn))
#define __packed __attribute__((packed))
#define __weak __attribute__((weak))
#define __deprecated __attribute__((deprecated))
#define __section(S) __attribute__((section(#S)))
#define __inittext __section(".init.text")
#define __initrodata __section(".init.rodata")
#define __initdata __section(".init.data")
#define __printf_like(n, m) __attribute__((__format__(__printf__, n, m)))
#define __compiler_offsetof(t, m) __builtin_offsetof(t, m)
#define __weak_alias(old, new) extern __typeof(old) new __attribute__((__weak__, __alias__(#old)))

#define __alias(old, new) extern __typeof(old) new __attribute__((__alias__(#old)))

#define __force
#define __must_check __attribute__((__warn_unused_result__))
#define __always_inline __inline__ __attribute__((__always_inline__))

#define uninitialized_var(x) x = x
#define fallthrough __attribute__((__fallthrough__))

#define __stringify_1(x...) #x
#define __stringify(x...) __stringify_1(x)

/************************类型定义******************************/

/************************接口声明******************************/

#ifdef __cplusplus
}
#endif /*__cplusplus */

#endif /*_COMPILER_H*/
