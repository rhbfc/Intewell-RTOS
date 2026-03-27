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

#include <errno.h>
#include <spinlock.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <system/types.h>
#include <ttosMM.h>
#include <ttosProcess.h>
#include <uaccess.h>

#undef KLOG_TAG
#define KLOG_TAG "uaccess"
#include <klog.h>

/************************宏 定 义******************************/
/*
 * Fault recovery is only wired up for built-in kernel uaccess helpers.
 * Loadable modules are outside this support boundary.
 */
#if defined(CONFIG_SUPPORT_FAULT_RECOVERY) && (defined(__aarch64__) || defined(__x86_64__) || defined(__arm__))
#define ARCH_UACCESS_FAULT_RECOVERY 1
#endif

/************************类型定义******************************/

/************************外部声明******************************/

/************************前向声明******************************/

/************************模块变量******************************/

/************************全局变量******************************/

/************************函数实现******************************/
/**
 * @brief
 *    从用户态地址空间拷贝数据到核心态地址空间
 * @param[in] to 核心态地址
 * @param[in] from 用户态地址
 * @param[in] n 拷贝的字节数
 * @retval
 *    0 拷贝成功
 *    EINVAL 拷贝失败
 */
int copy_from_user(void *to, const void __user *from, unsigned long n)
{
#ifdef ARCH_UACCESS_FAULT_RECOVERY
    return arch_copy_from_user(to, from, n);
#else
    if (0 == n)
    {
        return 0;
    }

    if (user_access_check(from, n, UACCESS_R) || kernel_access_check(from, n, UACCESS_R))
    {
        memcpy(to, from, n);
        return 0;
    }

    KLOG_E("copy_from_user fail, user addr:%p len:%lu error", from, n);

    return -EFAULT;
#endif
}

/**
 * @brief
 *    拷贝数据到用户态地址空间
 * @param[in] to 用户态地址
 * @param[in] from 核心态地址
 * @param[in] n 拷贝的字节数
 * @retval
 *    0 拷贝成功
 *    EINVAL 拷贝失败
 */
int copy_to_user(void __user *to, const void *from, unsigned long n)
{
#ifdef ARCH_UACCESS_FAULT_RECOVERY
    return arch_copy_to_user(to, from, n);
#else
    if (0 == n)
    {
        return 0;
    }

    if (user_access_check(to, n, UACCESS_W) || kernel_access_check(to, n, UACCESS_W))
    {
        memcpy(to, from, n);
        return 0;
    }

    KLOG_I("copy_to_user fail, user addr %p from %p error len:0x%lx error\n", to, from, n);

    return -EFAULT;
#endif
}

int strnlen_user(const char __user *str, size_t n)
{
#ifdef ARCH_UACCESS_FAULT_RECOVERY
    return arch_strnlen_user(str, n);
#else
    size_t len = 0;

    if (n == 0)
    {
        return 0;
    }

    while (len < n)
    {
        if (user_access_check(str, 1, UACCESS_R))
        {
            if (*str++ != '\0')
            {
                len++;
            }
            else
            {
                return len;
            }
        }
        else
        {
            KLOG_I("strnlen_user fail, user addr str %p error\n", (void *)str);
            break;
        }
    }

    return (len == n) ? (int)len : -EFAULT;
#endif
}

int strlen_user(const char __user *str)
{
    return strnlen_user(str, (size_t)-1);
}

int strcpy_from_user(char *dst, const char __user *src)
{
#ifdef ARCH_UACCESS_FAULT_RECOVERY
    return arch_strcpy_from_user(dst, src);
#else
    long len = 0;
    while (1)
    {
        if (user_access_check(src, 1, UACCESS_R))
        {
            if (*src != '\0')
            {
                len++;
                *dst++ = *src++;
            }
            else
            {
                *dst = '\0';
                return len;
            }
        }
        else
        {
            KLOG_I("strcpy_from_user fail, user addr %p error\n", (void *)src);
            break;
        }
    }

    return -EFAULT;
#endif
}

int strncpy_from_user(char *dst, const char __user *src, size_t n)
{
#ifdef ARCH_UACCESS_FAULT_RECOVERY
    return arch_strncpy_from_user(dst, src, n);
#else
    size_t len = 0;
    char ch;

    if (n == 0)
    {
        return 0;
    }

    while (len < n - 1)
    {
        if (user_access_check(src, 1, UACCESS_R))
        {
            ch = *src;
            *dst = ch;
            if (ch == '\0')
            {
                return (int)len;
            }

            dst++;
            src++;
            len++;
            continue;
        }

        KLOG_I("strncpy_from_user fail, user addr %p error\n", (void *)src);
        return -EFAULT;
    }

    *dst = '\0';
    return (int)len;
#endif
}

int strcpy_to_user(char __user *dst, const char *src)
{
#ifdef ARCH_UACCESS_FAULT_RECOVERY
    return arch_strcpy_to_user(dst, src);
#else
    long len = 0;
    while (1)
    {
        if (user_access_check(dst, 1, UACCESS_W))
        {
            if (*src != '\0')
            {
                len++;
                *dst++ = *src++;
            }
            else
            {
                *dst = '\0';
                return len;
            }
        }
        else
        {
            KLOG_I("strcpy_to_user fail, user addr str %p error\n", (void *)dst);
            break;
        }
    }

    return -EFAULT;
#endif
}

int strncpy_to_user(char __user *dst, const char *src, size_t n)
{
#ifdef ARCH_UACCESS_FAULT_RECOVERY
    return arch_strncpy_to_user(dst, src, n);
#else
    long len = 0;
    while (1)
    {
        if (user_access_check(dst, 1, UACCESS_W))
        {
            if (*src != '\0')
            {
                len++;
                *dst++ = *src++;
                if (len == (n - 1))
                {
                    *dst++ = '\0';
                    return len;
                }
            }
            else
            {
                return len;
            }
        }
        else
        {
            KLOG_I("strcpy_to_user fail, user addr str %p error\n", (void *)dst);
            break;
        }
    }

    return -EFAULT;
#endif
}

unsigned long clear_user(void __user *to, unsigned long n)
{
    if (n == 0)
    {
        return 0;
    }
    
#ifdef ARCH_UACCESS_FAULT_RECOVERY

    if (!access_ok(to, n))
    {
        return n;
    }

    return arch_clear_user(to, n);

#else

    if (user_access_check(to, n, UACCESS_W) || kernel_access_check(to, n, UACCESS_W))
    {
        memset(to, 0, n);
        return 0;
    }

    return n;
#endif
}

int access_process_vm(pcb_t pcb, unsigned long addr, void *buf, int len, int write)
{
    int copy_len = len;
    struct mm *mm = get_process_mm(pcb);
    phys_addr_t paddr = mm_v2p(mm, addr);
    if (paddr == 0ULL)
    {
        return -EINVAL;
    }

    if (!kernel_access_check(buf, len, write ? UACCESS_R : UACCESS_W))
    {
        return -EINVAL;
    }

    virt_addr_t kaddr = mm_kernel_p2v(paddr);

    /* 物理地址不在线性映射区 */
    if (kaddr == 0ULL)
    {
        /* 临时映射处理 */
        phys_addr_t off_in_page = paddr - PAGE_ALIGN_DOWN(paddr);
        while (len)
        {
            size_t copy_size = len + off_in_page > PAGE_SIZE ? PAGE_SIZE - off_in_page : len;
            kaddr = ktmp_access_start(PAGE_ALIGN_DOWN(paddr));
            if (write)
            {
                memcpy((void *)kaddr + off_in_page, buf, copy_size);
            }
            else
            {
                memcpy(buf, (void *)kaddr + off_in_page, copy_size);
            }
            off_in_page = 0;
            len -= copy_size;
            paddr += copy_size;
            ktmp_access_end(kaddr);
        }

        return copy_len;
    }

    if (!kernel_access_check((const void *)kaddr, len, write ? UACCESS_W : UACCESS_R))
    {
        return -EINVAL;
    }

    if (write)
    {
        memcpy((void *)kaddr, buf, len);
    }
    else
    {
        memcpy(buf, (void *)kaddr, len);
    }

    return len;
}
