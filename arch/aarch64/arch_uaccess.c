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

/**
 * @file arch_uaccess.c
 * @brief AArch64 用户态访问异常修复与封装接口。
 */

/************************头 文 件******************************/
#include <access_ok.h>
#include <errno.h>
#include <esr.h>
#include <mmu.h>
#include <sys/param.h>
#include <system/macros.h>
#include <system/types.h>
#include <uaccess.h>
#include <fault_recovery_encoding.h>
#include <fault_recovery_table.h>

#ifdef CONFIG_SUPPORT_FAULT_RECOVERY

extern int arch_copy_from_user_asm(void *to, const void __user *from, unsigned long n);
extern int arch_copy_to_user_asm(void __user *to, const void *from, unsigned long n);
extern int arch_strcpy_to_user_asm(char __user *dst, const char *src);
extern unsigned long arch_clear_user_asm(void __user *to, unsigned long n);

static inline void ex_set_reg(arch_exception_context_t *context, unsigned int reg, unsigned long val)
{
    if (reg < array_size(context->regs))
    {
        context->regs[reg] = val;
    }
}

static bool apply_resume_recovery(arch_exception_context_t *context,
                                  const fault_recovery_record_t *record)
{
    context->elr = fault_recovery_record_resume_pc(record);
    return true;
}

static bool apply_errno_clear_recovery(arch_exception_context_t *context,
                                       const fault_recovery_record_t *record)
{
    unsigned int reg_err = fault_recovery_record_status_slot(record);
    unsigned int reg_zero = fault_recovery_record_clear_slot(record);

    /* 按 fault-recovery 描述回填错误码与零值寄存器，并跳到恢复地址继续执行。 */
    ex_set_reg(context, reg_err, (unsigned long)-EFAULT);
    ex_set_reg(context, reg_zero, 0);
    return apply_resume_recovery(context, record);
}

static bool apply_data_abort_recovery(arch_exception_context_t *context,
                                      const fault_recovery_record_t *record)
{
    switch (record->action_code)
    {
    case FAULT_RECOVERY_ACTION_RESUME:
        return apply_resume_recovery(context, record);

    case FAULT_RECOVERY_ACTION_ERRNO_AND_CLEAR:
        return apply_errno_clear_recovery(context, record);

    default:
        return false;
    }
}

bool try_apply_fault_recovery(arch_exception_context_t *context)
{
    const fault_recovery_record_t *record;

    if (!esr_is_data_abort(context->esr))
    {
        return false;
    }

    record = find_fault_recovery_record(context->elr);
    if (record == NULL)
    {
        return false;
    }

    return apply_data_abort_recovery(context, record);
}

int arch_copy_from_user(void *to, const void __user *from, unsigned long n)
{
    if (n == 0)
    {
        return 0;
    }

    if (access_ok(from, n))
    {
        return arch_copy_from_user_asm(to, from, n);
    }

    return -EFAULT;
}

int arch_copy_to_user(void __user *to, const void *from, unsigned long n)
{
    if (n == 0)
    {
        return 0;
    }

    if (access_ok(to, n))
    {
        return arch_copy_to_user_asm(to, from, n);
    }

    return -EFAULT;
}

int arch_strnlen_user(const char __user *str, size_t n)
{
    size_t len = 0;
    char ch;

    if (n == 0)
    {
        return 0;
    }

    if (!access_ok(str, 1))
    {
        return -EFAULT;
    }

    while (len < n)
    {
        if (arch_copy_from_user(&ch, str, 1))
        {
            return -EFAULT;
        }

        if (ch == '\0')
        {
            return len;
        }

        str++;
        len++;
    }

    return (int)len;
}

int arch_strlen_user(const char __user *str)
{
    return arch_strnlen_user(str, (size_t)-1);
}

int arch_strcpy_from_user(char *dst, const char __user *src)
{
    int len = 0;
    char ch;

    if (!access_ok(src, 1))
    {
        return -EFAULT;
    }

    while (1)
    {
        if (arch_copy_from_user(&ch, src, 1))
        {
            return -EFAULT;
        }

        *dst++ = (char)ch;
        if (ch == '\0')
        {
            return len;
        }

        src++;
        len++;
    }
}

int arch_strncpy_from_user(char *dst, const char __user *src, size_t n)
{
    size_t len = 0;
    char ch;

    if (n == 0)
    {
        return 0;
    }

    if (!access_ok(src, 1))
    {
        return -EFAULT;
    }

    while (len < n - 1)
    {
        if (arch_copy_from_user(&ch, src, 1))
        {
            return -EFAULT;
        }

        *dst++ = ch;
        if (ch == '\0')
        {
            return (int)len;
        }

        src++;
        len++;
    }

    *dst = '\0';
    return (int)len;
}

int arch_strcpy_to_user(char __user *dst, const char *src)
{
    if (!access_ok(dst, 1))
    {
        return -EFAULT;
    }

    return arch_strcpy_to_user_asm(dst, src);
}

int arch_strncpy_to_user(char __user *dst, const char *src, size_t n)
{
    size_t len = 0;
    char ch;

    if (n == 0)
    {
        return 0;
    }

    if (!access_ok(dst, 1))
    {
        return -EFAULT;
    }

    while (len < n - 1)
    {
        ch = *src++;
        if (arch_copy_to_user(dst, &ch, 1))
        {
            return -EFAULT;
        }

        dst++;
        if (ch == '\0')
        {
            return len;
        }

        len++;
    }

    ch = '\0';
    if (arch_copy_to_user(dst, &ch, 1))
    {
        return -EFAULT;
    }

    return len;
}

unsigned long arch_clear_user(void __user *to, unsigned long n)
{
    if (n == 0)
    {
        return 0;
    }

    return arch_clear_user_asm(to, n);
}

#endif /* CONFIG_SUPPORT_FAULT_RECOVERY */

/**
 * @brief
 *    用户空间访问权限检查
 * @param[in] user_addr 用户态地址
 * @param[in] n 要检查的字节数
 * @param[in] flag 要检查的权限
 * @retval
 *   true 检查成功
 *   false 检查失败
 */
bool user_access_check(const void __user *user_addr, unsigned long n, int flag)
{
    unsigned long addr;
    unsigned long mmu_check_start;
    unsigned long mmu_check_size;

    if (!user_addr || !n || !access_ok(user_addr, n))
    {
        return false;
    }

    addr = (unsigned long)user_addr;
    mmu_check_start = PAGE_SIZE_ALIGN(addr);
    mmu_check_size = roundup((addr - mmu_check_start) + n, PAGE_SIZE);

    while (mmu_check_size > 0)
    {
        switch (flag)
        {
        case UACCESS_R:
            ats1e0r(mmu_check_start);
            break;

        case UACCESS_W:
            ats1e0w(mmu_check_start);
            break;

        case UACCESS_RW:
            ats1e0w(mmu_check_start);
            ats1e0r(mmu_check_start);
            break;

        default:
            return false;
        }

        if (PAR_F_MASK & read_par_el1())
        {
            return false;
        }

        mmu_check_start += PAGE_SIZE;
        mmu_check_size -= PAGE_SIZE;
    }

    return true;
}

/**
 * @brief
 *    内核空间访问权限检查
 * @param[in] user_addr 用户态地址
 * @param[in] n 要检查的字节数
 * @param[in] flag 要检查的权限
 * @retval
 *   true 检查成功
 *   false 检查失败
 */
bool kernel_access_check(const void *kernel_addr, unsigned long n, int flag)
{
    unsigned long addr;
    unsigned long mmu_check_start;
    unsigned long mmu_check_size;

    if (!kernel_addr || !n)
    {
        return false;
    }

    addr = (unsigned long)kernel_addr;
    mmu_check_start = PAGE_SIZE_ALIGN(addr);
    mmu_check_size = roundup((addr - mmu_check_start) + n, PAGE_SIZE);

    while (mmu_check_size > 0)
    {
        switch (flag)
        {
        case UACCESS_R:
            ats1e1r(mmu_check_start);
            break;

        case UACCESS_W:
            ats1e1w(mmu_check_start);
            break;

        case UACCESS_RW:
            ats1e1w(mmu_check_start);
            ats1e1r(mmu_check_start);
            break;

        default:
            return false;
        }

        if (PAR_F_MASK & read_par_el1())
        {
            return false;
        }

        mmu_check_start += PAGE_SIZE;
        mmu_check_size -= PAGE_SIZE;
    }

    return true;
}
