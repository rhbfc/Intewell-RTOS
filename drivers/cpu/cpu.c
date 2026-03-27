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

#include <driver/cpudev.h>
#include <driver/driver.h>
#include <list.h>
#include <stdio.h>
#include <string.h>
#include <time/ktime.h>

#define KLOG_TAG "CPU"
#include "klog.h"

static __aligned(8) LIST_HEAD(cpu_list);
static uint32_t cpu_numbers = 0;
static const char cpu_state_string[][16] = {
    [CPU_STATE_SHUTDOWN] = "SHUTDOWN",
    [CPU_STATE_RUNNING] = "RUNNING",
    [CPU_STATE_SUSPEND] = "SUSPEND",
};

int cpu_method_copy(struct cpudev *cpu)
{
    struct cpudev *other_cpu;

    list_for_each_entry(other_cpu, &cpu_list, node)
    {
        if (other_cpu == cpu)
            continue;

        if (cpu->dev_ops)
        {
            if (other_cpu->cluster_id == cpu->cluster_id)
            {
                memcpy(&cpu->method, &other_cpu->method, sizeof(struct cpu_method));
                break;
            }
            else
            {
                return -1;
            }
        }
        else
        {
            memcpy(&cpu->method, &other_cpu->method, sizeof(struct cpu_method));
            break;
        }
    }

    return 0;
}

void cpu_add_to_list(struct cpudev *cpu)
{
    struct cpudev *other_cpu;

    INIT_LIST_HEAD(&cpu->node);
    cpu->cpu_cluster_numbers = 1;

    cpu_numbers++;
    cpu->cpu_numbers = &cpu_numbers;

    list_add_head(&cpu->node, &cpu_list);

    list_for_each_entry(other_cpu, &cpu_list, node)
    {
        if (other_cpu == cpu)
            continue;

        if (cpu->dev_ops)
        {
            if (other_cpu->cluster_id == cpu->cluster_id)
            {
                other_cpu->cpu_cluster_numbers++;
                cpu->cpu_cluster_numbers = other_cpu->cpu_cluster_numbers;
            }
        }
        else
        {
            other_cpu->cpu_cluster_numbers++;
            cpu->cpu_cluster_numbers = other_cpu->cpu_cluster_numbers;
        }
    }

    if (cpu->boot_cpu)
    {
        cpu->state = CPU_STATE_RUNNING;
        cpuid_set();
    }
}

struct cpudev *cpu_boot_get(void)
{
    struct cpudev *cpu;

    list_for_each_entry(cpu, &cpu_list, node)
    {
        if (cpu->boot_cpu)
            return cpu;
    }

    return NULL;
}

struct cpudev *_cpu_self_get(void)
{
    struct cpudev *cpu;

    list_for_each_entry(cpu, &cpu_list, node)
    {
        if (cpu->dev_ops->cpu_is_self(cpu))
        {
            return cpu;
        }
    }

    return NULL;
}

uint64_t cpu_hw_id_get(struct cpudev *cpu)
{
    return cpu->dev_ops->cpu_hw_id_get(cpu);
}

struct list_head *cpu_list_get(void)
{
    return &cpu_list;
}

int cpu_numbers_get(void)
{
    return cpu_numbers;
}

void cpu_enter_idle(void)
{
    struct cpudev *cpu = cpu_self_get();
    struct timespec64 ts;
    if (unlikely(cpu == NULL))
    {
        return;
    }
    kernel_clock_gettime(CLOCK_MONOTONIC, &cpu->last_enter_idle);
    ts = clock_timespec_subtract64(&cpu->last_enter_idle, &cpu->last_enter_sys);
    cpu->stime = clock_timespec_add64(&cpu->stime, &ts);
}

void cpu_leave_idle(void)
{
    struct cpudev *cpu = cpu_self_get();
    struct timespec64 ts;
    if (unlikely(cpu == NULL))
    {
        return;
    }
    kernel_clock_gettime(CLOCK_MONOTONIC, &cpu->last_enter_sys);
    ts = clock_timespec_subtract64(&cpu->last_enter_sys, &cpu->last_enter_idle);
    cpu->itime = clock_timespec_add64(&cpu->itime, &ts);
}

void cpu_enter_user(void)
{
    struct cpudev *cpu = cpu_self_get();
    struct timespec64 ts;
    if (unlikely(cpu == NULL))
    {
        return;
    }
    kernel_clock_gettime(CLOCK_MONOTONIC, &cpu->last_enter_user);

    ts = clock_timespec_subtract64(&cpu->last_enter_user, &cpu->last_enter_sys);
    cpu->stime = clock_timespec_add64(&cpu->stime, &ts);
}

void cpu_leave_user(void)
{
    struct cpudev *cpu = cpu_self_get();
    struct timespec64 ts;
    if (unlikely(cpu == NULL))
    {
        return;
    }
    kernel_clock_gettime(CLOCK_MONOTONIC, &cpu->last_enter_sys);

    ts = clock_timespec_subtract64(&cpu->last_enter_sys, &cpu->last_enter_user);
    cpu->utime = clock_timespec_add64(&cpu->utime, &ts);
}

static void show_cpu_item(struct cpudev *cpu)
{
    if (cpu == NULL)
    {
        printf("%-*.*s %-*.*s %-*.*s %-*.*s %-*.*s %-*.*s\r\n", 16, ' ', "ARCH", 4, ' ', "ID", 12,
               ' ', "CLUSTER.CPU", 16, ' ', "METHOD", 8, ' ', "BOOT", 16, ' ', "STATE");
    }
    else
    {
        printf("%-16s %-4d %7lld.%lld    %-16s    %c     %-*.*s\r\n", cpu->name, cpu->index,
               cpu->cluster_id, cpu->code_id, cpu->method.ops->name, (cpu->boot_cpu) ? 'Y' : ' ',
               16, ' ', cpu_state_string[cpu->state]);
    }
}

#ifdef CONFIG_SHELL
#include <shell.h>
static void show_cpus(int argc, const char *argv[])
{
    struct cpudev *cpu;

    show_cpu_item(NULL);
    list_for_each_entry(cpu, &cpu_list, node)
    {
        show_cpu_item(cpu);
    }
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) |
                     SHELL_CMD_DISABLE_RETURN,
                 lscpu, show_cpus, cpu list);

static void cpu_reboot(int argc, const char *argv[])
{
    cpu_reset(cpu_boot_dev_get());
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) |
                     SHELL_CMD_DISABLE_RETURN,
                 reboot, cpu_reboot, reboot system);
#endif