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

#ifndef __DRV_CPU_H__
#define __DRV_CPU_H__

#include <cpuid.h>
#include <driver/device.h>
#include <errno.h>
#include <limits.h>
#include <list.h>
#include <stdint.h>
#include <time.h>

enum
{
    CPU_STATE_SHUTDOWN = 0,
    CPU_STATE_START_UP,
    CPU_STATE_RUNNING,
    CPU_STATE_SUSPEND,
};

enum
{
    PSCI01_MIGRATE = 0,
    PSCI01_ON,
    PSCI01_OFF,
    PSCI01_SUSPEND,
};

typedef int (*psci_func_t)(size_t arg0, size_t arg1, size_t arg2, size_t arg3);

struct cpudev;

struct cpu_dev_ops
{
    int (*cpu_affinity_set)(struct device *dev);
    bool (*cpu_is_self)(struct cpudev *cpu);
    uint64_t (*cpu_hw_id_get)(struct cpudev *cpu);
    void (*cpu_idle)(struct cpudev *cpu);
};

struct cpu_ops
{
    char *name;

    int (*init)(struct cpudev *cpu);
    int (*cpu_on)(struct cpudev *cpu, phys_addr_t entry, unsigned long long context_id);
    int (*cpu_off)(struct cpudev *cpu);
    int (*cpu_suspend)(struct cpudev *cpu, unsigned int power_state, phys_addr_t entry,
                       unsigned long long context_id);
    int (*cpu_migrate)(struct cpudev *cpu, phys_addr_t entry, unsigned long long context_id);
    int (*cpu_reset)(struct cpudev *cpu);
    int (*system_poweroff)(struct cpudev *cpu);
};

struct cpu_method
{
    unsigned int psci01_code[4];
    psci_func_t psci_func;
    struct cpu_ops *ops;
};

#ifdef __x86_64__
#define X86_VENDOR_INTEL 0
#define X86_VENDOR_AMD 1
#define X86_VENDOR_HYGON 2
#define X86_VENDOR_UNKNOWN 0xff

#define X86_VENDOR_STRING_INTEL "GenuineIntel"
#define X86_VENDOR_STRING_AMD "AuthenticAMD"
#define X86_VENDOR_STRING_HYGON "HygonGenuine"
#endif

struct cpudev
{
    struct list_head node;
    char *name;
    bool boot_cpu;
    volatile unsigned int state;      /* cpu 状态 */
    unsigned int cpu_cluster_numbers; /* cpu 同簇内核心数 */
    unsigned int *cpu_numbers;        /* cpu 上总核心数 */
    unsigned int index;               /* cpu 逻辑序号 (设备树顺序) */
    unsigned long long code_id;
    unsigned long long cluster_id;
    unsigned long long release_addr;
    struct timespec64 last_enter_user;
    struct timespec64 last_enter_sys;
    struct timespec64 last_enter_idle;
    struct timespec64 utime;
    struct timespec64 stime;
    struct timespec64 itime;
    struct cpu_method method;
#ifdef __x86_64__
    uint32_t x86_vendor;
#endif
    struct cpu_dev_ops *dev_ops;
    void *priv;
};

int cpu_method_copy(struct cpudev *cpu);
void cpu_add_to_list(struct cpudev *cpu);
struct cpudev *cpu_boot_get(void);
struct cpudev *_cpu_self_get(void);
int cpu_numbers_get(void);
struct list_head *cpu_list_get(void);

void cpu_enter_idle(void);
void cpu_leave_idle(void);
void cpu_enter_user(void);
void cpu_leave_user(void);

static inline __attribute__((always_inline)) int
cpu_on(struct cpudev *cpu, unsigned long long entry, unsigned long context_id)
{
    if (cpu->method.ops && cpu->method.ops->cpu_on)
        return cpu->method.ops->cpu_on(cpu, entry, context_id);

    return -EIO;
}

static inline __attribute__((always_inline)) int cpu_reset(struct cpudev *cpu)
{
    if (cpu && cpu->method.ops && cpu->method.ops->cpu_reset)
        return cpu->method.ops->cpu_reset(cpu);

    return -EIO;
}

static inline __attribute__((always_inline)) int system_poweroff()
{
    struct cpudev *cpu = _cpu_self_get();
    if (cpu && cpu->method.ops && cpu->method.ops->system_poweroff)
        return cpu->method.ops->system_poweroff(cpu);

    return -EIO;
}

static inline __attribute__((always_inline)) void cpu_idle(void)
{
    struct cpudev *cpu = _cpu_self_get();

    if (cpu && cpu->dev_ops && cpu->dev_ops->cpu_idle)
        cpu->dev_ops->cpu_idle(cpu);
}

static inline __attribute__((always_inline)) struct cpudev *cpu_boot_dev_get(void)
{
    return cpu_boot_get();
}

static inline __attribute__((always_inline)) struct cpudev *cpu_self_get(void)
{
    return _cpu_self_get();
}

uint64_t cpu_hw_id_get(struct cpudev *cpu);

/* 链表迭代节点对应的结构体 */
#define for_each_present_cpu(cpu)                                                                  \
    struct list_head *head = cpu_list_get();                                                       \
    for (cpu = list_entry(head->prev, typeof(struct cpudev), node); &cpu->node != head;            \
         cpu = list_entry(cpu->node.prev, typeof(struct cpudev), node))

#endif /* __DRIVER_H__ */
