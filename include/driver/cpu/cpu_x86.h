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

#ifndef _CPU_X86_H
#define _CPU_X86_H

/************************头文件********************************/
#include <driver/device.h>
#include <system/macros.h>
#include <system/types.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/************************类型定义******************************/
struct x86_cpu_priv
{
    uint64_t apicid;
};

/************************接口声明******************************/

void cpu_method_ops_match(struct device *dev);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _CPU_X86_H */
