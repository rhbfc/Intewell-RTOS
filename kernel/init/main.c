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

/************************头 文 件******************************/
#include <cache.h>
#include <cpu.h>
#include <cpuid.h>
#include <driver/cpudev.h>
#include <page.h>
#include <smp.h>
#include <stdio.h>
#include <ttos.h>
#include <ttosMM.h>
#include <ttos_pic.h>
#include <ttos_time.h>
#include <system/kconfig.h>

#undef KLOG_TAG
#define KLOG_TAG "main"
#include <klog.h>
#include <ttos_init.h>
#include <time/ktimer.h>

/************************宏 定 义******************************/
#define CONFIG_SYS_STACK_SIZE 8192

/************************类型定义******************************/
/************************外部声明******************************/
extern s32 arch_timer_pre_init(void);
extern T_TTOS_ConfigTable ttosConfigTable;

void arch_timer_startup(void);
int  clockchip_init(void);
void earlycon_init(void);
void ap_arch_init(void);
void bp_arch_init(void);
int  arch_timer_clockchip_init(void);

/************************前向声明******************************/
/************************模块变量******************************/
/************************全局变量******************************/
unsigned char tmp_stack[PAGE_SIZE * CONFIG_MAX_CPUS*2] __attribute__((aligned(PAGE_SIZE)));
unsigned char sys_stack[CONFIG_SYS_STACK_SIZE] __attribute__((aligned(16)));
void *running_task_stack[CONFIG_MAX_CPUS] = {};
void *running_task[CONFIG_MAX_CPUS] = {};

/************************函数实现******************************/
T_UWORD cpuNumGet(T_VOID)
{
    return CONFIG_MAX_CPUS;
}

T_UBYTE pthreadPriorityToCore(int priority)
{
    return (T_UBYTE)(255 - priority);
}

T_UBYTE corePriorityToPthread(int priority)
{
    return (T_UBYTE)(255 - priority);
}

/*
 * @brief
 *     设置当前任务核心态栈。
 * @param[in]: sysStack: 栈空间地址。
 * @returns
 *     none
 */
T_VOID sysStackSet(T_VOID *sysStack)
{
    T_WORD cpuid = 0;
    cpuid = cpuid_get();
    void *sp = (void *)(((ADDRESS)sysStack - 32) & (~0xful));

#ifdef __x86_64__
    /* 设置在用户态产生中断时的栈，系统调用的栈在汇编入口处通过swapgs来获取 */
    void tss_set_stack(uint32_t cpuid, uint64_t addr);
    tss_set_stack(cpuid, (uint64_t)sp);
#endif

    /* 保证栈地址是16字节对齐 */
    running_task_stack[cpuid] = sp;
}

/*
 * @brief
 *     获取当前任务核心态栈。
 * @param[in]: sysStack: 栈空间地址。
 * @returns
 *     none
 */
void *sysStackGet(void)
{
    unsigned int cpuid = cpuid_get();

    /* 保证栈地址是16字节对齐 */
    return running_task_stack[cpuid];
}

/*
 * @brief:
 *    中断异常临时栈设置，后续跟task模块一起修改。
 * @param[in]: stack
 * @return:
 *    无
 */
void sys_stack_init(void *stack)
{
    u32 cpuid = 0;
    cpuid = cpuid_get();

    running_task_stack[cpuid] = (void *)(((size_t)stack - 32) & (~0xful));
}

__weak void bp_arch_init(void) {}
__weak void ap_arch_init(void) {}

/*
 * @brief:
 *    系统初始化。
 * @param[in]: 无
 * @return:
 *    无
 */

void start_kernel(void)
{
    /* 早期串口初始化 */
    earlycon_init();

    /* 架构初始化 */
    bp_arch_init();

    /* 初始化MMU和页分配器 */
    ttosMemoryManagerInit();

    ttos_pre_init();

    sys_stack_init(
        (void *)page_address(pages_alloc(page_bits(CONFIG_SYS_STACK_SIZE), ZONE_NORMAL)));

    /* 注册arch timer ops,获取timer freq */
    arch_timer_pre_init();

    /* pic相关初始化 */
    ttos_pic_init(NULL);

    ktimer_sys_init();

    /* arch timer初始化 */
    ttos_time_init();

    clockchip_init();

    ttos_time_enable();

    ttos_arch_init();

    TTOS_StartOS(&ttosConfigTable, 0);
    while (1)
        ;
}

/*
 * @brief:
 *    AP启动后，入口函数，完成相关初始化操作
 * @param[in] 无
 * @return 无
 */
void start_ap(void)
{
    struct cpudev *cpu;

    /* 架构初始化 */
    ap_arch_init();

    cpu = cpu_self_get();

    cache_dcache_invalidate((size_t)cpu, sizeof(struct cpudev));

    cpuid_set();

    /* 设置从核启动完成状态 */
    smp_slave_init(cpu);

    ttos_pic_init(NULL);

    ktimer_sys_init();

    arch_timer_clockchip_init();

    ttos_time_init();

    ttos_time_enable();

    ttosAPEntry();
    while (1)
        ;
}
