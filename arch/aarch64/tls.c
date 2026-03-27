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
 * @file tls.c
 * @brief AArch64 TLS 寄存器访问接口。
 */

#include <arch_helpers.h>
#include <stdint.h>

/* 设置当前线程的用户态 TLS 基址。 */
void kernel_set_tls (uintptr_t tls)
{
    write_tpidr_el0(tls);
}

/* 读取当前线程的用户态 TLS 基址。 */
uintptr_t kernel_get_tls (void)
{
    return read_tpidr_el0();
}
