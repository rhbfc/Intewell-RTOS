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
 * @file： psci.h
 * @brief：
 *	    <li>提供PSCI相关宏定义。</li>
 */

#ifndef _PSCI_H
#define _PSCI_H

/************************头文件********************************/
#include <system/types.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/************************宏定义********************************/

#define PSCI_FN_32_64(code)     (code | ((sizeof(void*) == 8) ? 0x40000000u : 0))
#define PSCI_FN_32(code)        (code)

/* PSCI功能ID */
#define PSCI_VERSION             PSCI_FN_32     (0x84000000u)
#define PSCI_CPU_SUSPEND         PSCI_FN_32_64  (0x84000001u)
#define PSCI_CPU_OFF             PSCI_FN_32     (0x84000002u)
#define PSCI_CPU_ON              PSCI_FN_32_64  (0x84000003u)
#define PSCI_AFFINITY_INFO       PSCI_FN_32_64  (0x84000004u)
#define PSCI_MIGRATE             PSCI_FN_32_64  (0x84000005u)
#define PSCI_SYSTEM_OFF          PSCI_FN_32     (0x84000008u)
#define PSCI_SYSTEM_RESET        PSCI_FN_32     (0x84000009u)
#define PSCI_FEATURES            PSCI_FN_32     (0x8400000Au)
#define PSCI_SMCCC_VERSION_FUNC  PSCI_FN_32     (0x80000000u)

/* PSCI错误码 */
#define PSCI_SUCCESS            0
#define PSCI_NOT_SUPPORTED      -1
#define PSCI_INVALID_PARAMETERS -2
#define PSCI_DENIED             -3
#define PSCI_ALREADY_ON         -4
#define PSCI_ON_PENDING         -5
#define PSCI_INTERNAL_FAILURE   -6
#define PSCI_NOT_PRESENT        -7
#define PSCI_DISABLED           -8
#define PSCI_INVALID_ADDRESS    -9

/* PSCI版本号宏 */
#define PSCI_MAJOR_VERSION_SHIFT 16
#define PSCI_MINOR_VERSION_MASK  ((1U << PSCI_MAJOR_VERSION_SHIFT) - 1)
#define PSCI_MAJOR_VERSION_MASK  ~PSCI_MINOR_VERSION_MASK
#define PSCI_MAJOR_VERSION(version)                                            \
    (((version)&PSCI_MAJOR_VERSION_MASK) >> PSCI_MAJOR_VERSION_SHIFT)
#define PSCI_MINOR_VERSION(version) ((version)&PSCI_MINOR_VERSION_MASK)
#define PSCI_MAKE_VERSION(major, minor)                                        \
    (((major) << PSCI_MAJOR_VERSION_SHIFT) | (minor))

/************************类型定义******************************/

/************************接口声明******************************/

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _PSCI_H */
