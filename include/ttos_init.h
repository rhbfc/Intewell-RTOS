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
 * @file： cpuid.h
 * @brief：
 *	    <li>提供cpuid相关功能函数。</li>
 */

#ifndef __TTOS_INIT_H__
#define __TTOS_INIT_H__

#ifndef INIT_SECTION
#if defined(__CC_ARM) || defined(__CLANG_ARM)
#define INIT_SECTION(x) __attribute__((section(x)))
#elif defined(__GNUC__)
#define INIT_SECTION(x) __attribute__((section(x)))
#else
#define INIT_SECTION(x)
#endif
#endif

#ifndef INIT_USED
#if defined(__CC_ARM) || defined(__CLANG_ARM)
#define INIT_USED __attribute__((used))
#elif defined(__GNUC__)
#define INIT_USED __attribute__((used))
#else
#define INIT_USED
#endif
#endif

/* 前段初始化 此阶段将在初始化完MMU后进行，此阶段可进行内存分配，不可创建线程 */
#define INIT_STAGE_PRE 0

/* 架构初始化 此阶段将在初始化完中断控制器与系统定时器后进行，不可创建线程 */
#define INIT_STAGE_ARCH 1

/* 系统初始化 此阶段将在初始化操作系统内核后进行，此阶段可创建线程 */
#define INIT_STAGE_SYS 2

struct init_data
{
    int (*function)(void);
    const char *func_name;
    const char *desc_str;
    const char *pad;
};

int ttos_pre_init(void);
int ttos_arch_init(void);
int ttos_sys_init(void);

// clang-format off
#define GET_INIT_TABLE(func) ((struct init_data *)(&func##_init_data))

#define INIT_EXPORT_SECTION(func, stage, substage, desc)                       \
    const struct init_data INIT_USED INIT_SECTION (                            \
        ".ttos_init." #stage "." #substage "." #func) func##_init_data         \
        = { .function = func, .func_name = #func, .desc_str = desc }

#define EXPORT_INIT_SUBSTAGE(func, stage, substage, desc)   INIT_EXPORT_SECTION (func, stage, substage, desc)

#define INIT_EXPORT_SECTION_START(func, stage)              EXPORT_INIT_SUBSTAGE (func, stage, 0.0, #func)
#define INIT_EXPORT_SECTION_END(func, stage)                EXPORT_INIT_SUBSTAGE (func, stage, 5.9, #func)

#define INIT_EXPORT_PRE(func, desc)                         EXPORT_INIT_SUBSTAGE (func, INIT_STAGE_PRE,  0.1, desc)
#define INIT_EXPORT_PRE_DEVSYS(func, desc)                  EXPORT_INIT_SUBSTAGE (func, INIT_STAGE_PRE,  0.2, desc)
#define INIT_EXPORT_PRE_PLATFORM(func, desc)                EXPORT_INIT_SUBSTAGE (func, INIT_STAGE_PRE,  0.3, desc)
#define INIT_EXPORT_PRE_CPU(func, desc)                     EXPORT_INIT_SUBSTAGE (func, INIT_STAGE_PRE,  1.0, desc)
#define INIT_EXPORT_PRE_PIC(func, desc)                     EXPORT_INIT_SUBSTAGE (func, INIT_STAGE_PRE,  1.2, desc)

#define INIT_EXPORT_ARCH(func, desc)                        EXPORT_INIT_SUBSTAGE (func, INIT_STAGE_ARCH, 0.1, desc)

#define INIT_EXPORT_SYS(func, desc)                         EXPORT_INIT_SUBSTAGE (func, INIT_STAGE_SYS,  0.1, desc)
#define INIT_EXPORT_SUBSYS(func, desc)                      EXPORT_INIT_SUBSTAGE (func, INIT_STAGE_SYS,  0.2, desc)
#define INIT_EXPORT_COMPONENTS(func, desc)                  EXPORT_INIT_SUBSTAGE (func, INIT_STAGE_SYS,  0.4, desc)
#define INIT_EXPORT_DEV_BUS(func, desc)                     EXPORT_INIT_SUBSTAGE (func, INIT_STAGE_SYS,  0.5, desc)
#define INIT_EXPORT_PRE_DRIVER(func, desc)                  EXPORT_INIT_SUBSTAGE (func, INIT_STAGE_SYS,  0.6, desc)
#define INIT_EXPORT_DRIVER(func, desc)                      EXPORT_INIT_SUBSTAGE (func, INIT_STAGE_SYS,  0.7, desc)
#define INIT_EXPORT_SERVE(func, desc)                       EXPORT_INIT_SUBSTAGE (func, INIT_STAGE_SYS,  1.0, desc)
#define INIT_EXPORT_ROOT_FS(func, desc)                     EXPORT_INIT_SUBSTAGE (func, INIT_STAGE_SYS,  2.0, desc)
#define INIT_EXPORT_INITRD(func, desc)                      EXPORT_INIT_SUBSTAGE (func, INIT_STAGE_SYS,  2.1, desc)
#define INIT_EXPORT_SERVE_FS(func, desc)                    EXPORT_INIT_SUBSTAGE (func, INIT_STAGE_SYS,  2.2, desc)
#define INIT_EXPORT_USER(func, desc)                        EXPORT_INIT_SUBSTAGE (func, INIT_STAGE_SYS,  5.0, desc)

#endif /* __TTOS_INIT_H__ */
