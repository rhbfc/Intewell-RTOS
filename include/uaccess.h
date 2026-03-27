/************************************************************************
 * Copyright (c) 2026 Kyland Inc.
 * Intewell-RTOS is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 ***********************************************************************/

#ifndef _UACCESS_H
#define _UACCESS_H

/************************头文件********************************/
#include <cpu.h>
#include <stdbool.h>
#include <system/types.h>
#include <access_ok.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef __user
#define __user
#endif

#ifndef __chk_user_ptr
#define __chk_user_ptr(ptr) ((void)(ptr))
#endif

/************************宏定义********************************/
#define UACCESS_R 1
#define UACCESS_W 2
#define UACCESS_RW 4

/************************类型定义******************************/

/************************接口声明******************************/
bool user_access_check(const void __user *user_addr, unsigned long n, int flag);
bool kernel_access_check(const void *kernel_addr, unsigned long n, int flag);
int copy_from_user(void *to, const void __user *from, unsigned long n);
int copy_to_user(void __user *to, const void *from, unsigned long n);
int strnlen_user(const char __user *str, size_t n);
int strlen_user(const char __user *str);
int strcpy_from_user(char *dst, const char __user *src);
int strncpy_from_user(char *dst, const char __user *src, size_t n);
int strcpy_to_user(char __user *dst, const char *src);
int strncpy_to_user(char __user *dst, const char *src, size_t n);
unsigned long clear_user(void __user *to, unsigned long n);

#define user_string_access_ok(str) (strlen_user(str) > 0)

/**
 * pcb  被调试的进程
 * addr 要访问的进程的虚拟地址
 * buf  要操作的数据的地址
 * len  数据长度
 * write 1:向进程写数据，0:从进程读数据
 */
typedef struct T_TTOS_ProcessControlBlock *pcb_t;
int access_process_vm(pcb_t pcb, unsigned long addr, void *buf, int len, int write);

/*
 * Fault recovery is intended for built-in kernel uaccess helpers.
 * Loadable modules are not part of this contract.
 */
#if defined(CONFIG_SUPPORT_FAULT_RECOVERY) && (defined(__aarch64__) || defined(__x86_64__) || defined(__arm__))
int arch_copy_from_user(void *to, const void __user *from, unsigned long n);
int arch_copy_to_user(void __user *to, const void *from, unsigned long n);
int arch_strnlen_user(const char __user *str, size_t n);
int arch_strlen_user(const char __user *str);
int arch_strcpy_from_user(char *dst, const char __user *src);
int arch_strncpy_from_user(char *dst, const char __user *src, size_t n);
int arch_strcpy_to_user(char __user *dst, const char *src);
int arch_strncpy_to_user(char __user *dst, const char *src, size_t n);
unsigned long arch_clear_user(void __user *to, unsigned long n);
#endif

#ifdef __aarch64__
#include <arch/aarch64/arch_uaccess.h>
#endif
#ifdef __arm__
#include <arch/arm/arch_uaccess.h>
#endif
#ifdef __x86_64__
#include <arch/x86_64/arch_uaccess.h>
#endif

#ifndef put_user
#ifdef __put_user
#define put_user(x, ptr) __put_user((x), (ptr))
#else
#define put_user(x, ptr)                                                                           \
    ({                                                                                             \
        __typeof__(*(ptr)) __pu_val = (__typeof__(*(ptr)))(x);                                    \
        __chk_user_ptr(ptr);                                                                       \
        copy_to_user((ptr), &__pu_val, sizeof(__pu_val));                                         \
    })
#endif
#endif

#ifndef get_user
#ifdef __get_user
#define get_user(x, ptr) __get_user((x), (ptr))
#else
#define get_user(x, ptr)                                                                           \
    ({                                                                                             \
        const __typeof__(*(ptr)) __user *__gu_ptr = (ptr);                                        \
        __typeof__(*(ptr)) __gu_val = 0;                                                          \
        __chk_user_ptr(ptr);                                                                       \
        int __gu_ret = copy_from_user(&__gu_val, __gu_ptr, sizeof(*__gu_ptr));                    \
        (x) = (__typeof__(x))__gu_val;                                                             \
        __gu_ret;                                                                                  \
    })
#endif
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* _UACCESS_H */
