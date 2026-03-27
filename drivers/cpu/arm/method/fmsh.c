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

#include <driver/cpu/cpu_arm.h>
#include <driver/devbus.h>
#include <driver/device.h>
#include <driver/driver.h>
#include <driver/of.h>
#include <inttypes.h>
#include <io.h>
#include <ttosMM.h>

#undef KLOG_TAG
#define KLOG_TAG "FMSH OPS"
#include "klog.h"

static devaddr_region_t fmsh_slcr_addr = {0};

#define SLCR_UNLOCK_OFFSET 0x8 /* SCLR unlock register */
#define SLCR_UNLOCK_MAGIC 0xDF0D767B
#define SLCR_PS_RST_CTRL_OFFSET 0x200   /* PS Software Reset Control */
#define SLCR_REBOOT_STATUS_OFFSET 0x400 /* PS Reboot Status */
#define CORE0_ENTRY_OFFSET 0x438

static void fmsh_slcr_write(u32 val, u32 offset)
{
    writel(val, fmsh_slcr_addr.vaddr + offset);
}

static uint32_t fmsh_slcr_read(u32 offset)
{
    return readl(fmsh_slcr_addr.vaddr + offset);
}

static inline int fmsh_slcr_unlock(void)
{
    fmsh_slcr_write(SLCR_UNLOCK_MAGIC, SLCR_UNLOCK_OFFSET);
    return 0;
}

/**
 * fmsh_slcr_cpu_state - Read/write cpu state
 * @cpu:	cpu number
 * @die:	cpu state - true if cpu is going to die
 *
 * SLCR_REBOOT_STATUS save upper 4 bits (31:28 cpu states for cpu[0:3])
 * 0 means cpu is running, 1 cpu is going to die.
 */
static void fmsh_slcr_cpu_state_write(int cpu, bool die)
{
    u32 state, mask;

    state = fmsh_slcr_read(SLCR_REBOOT_STATUS_OFFSET);
    mask = 1 << (31 - cpu);
    if (die)
        state |= mask;
    else
        state &= ~mask;
    fmsh_slcr_write(state, SLCR_REBOOT_STATUS_OFFSET);
}

static int fmsh_cpu_on(struct cpudev *cpu, phys_addr_t entry, unsigned long long context_id)
{
    u32 state, mask;

    KLOG_I("smp: write entry:0x%llx to reg: 0x%lx for wakeup cpu%lld", entry,
           fmsh_slcr_addr.paddr + CORE0_ENTRY_OFFSET + (context_id * 8), context_id);

    /* 设置启动地址 */
    fmsh_slcr_write(entry, CORE0_ENTRY_OFFSET + (context_id * 8));

    /* 设置状态为runing*/
    fmsh_slcr_cpu_state_write(context_id, false);

    smp_wmb();
    __asm__ __volatile__("sev" ::: "memory");
}

static int fmsh_cpu_off(struct cpudev *cpu)
{
    printk("%s %d not implemented\n", __func__, __LINE__);

    // u32 state, mask;

    // /* 设置状态为die*/
    // fmsh_slcr_cpu_state_write(cpu->index, true);
    // /* clear boot address */
    // fmsh_slcr_write(0x0, CORE0_ENTRY_OFFSET + (cpu->index * 8));

    return 0;
}

static int fmsh_cpu_suspend(struct cpudev *cpu, unsigned int power_state, phys_addr_t entry,
                            unsigned long long context_id)
{
    printk("%s %d not implemented\n", __func__, __LINE__);
    return 0;
}

static int fmsh_cpu_migrate(struct cpudev *cpu, phys_addr_t entry, unsigned long long context_id)
{
    printk("%s %d not implemented\n", __func__, __LINE__);
    return 0;
}

static int fmsh_cpu_reset(struct cpudev *cpu)
{
    u32 reboot;

    /*
     * Clear 0x0F000000 bits of reboot status register to workaround
     * the FSBL not loading the bitstream after soft-reboot
     * This is a temporary solution until we know more.
     */
    reboot = fmsh_slcr_read(SLCR_REBOOT_STATUS_OFFSET);
    fmsh_slcr_write(reboot & 0xF0FFFFFF, SLCR_REBOOT_STATUS_OFFSET);
    fmsh_slcr_write(1, SLCR_PS_RST_CTRL_OFFSET);
    return 0;
}

static int fmsh_system_poweroff(struct cpudev *cpu)
{
    printk("%s %d not implemented\n", __func__, __LINE__);
    return 0;
}

static int fmsh_ops_init(struct cpudev *cpu)
{
    struct device dev;
    struct device_node *np;
    int ret = 0;
    static bool slcr_is_enabled = 0;

    if (slcr_is_enabled == 1)
    {
        return 0;
    }

    np = of_find_compatible_node(NULL, NULL, "fmsh,psoc-slcr");
    if (!np)
    {
        KLOG_E("no slcr node found");
    }

    dev.of_node = np;

    ret = of_device_get_resource_reg(&dev, &fmsh_slcr_addr.paddr, &fmsh_slcr_addr.size);
    if (ret < 0)
    {
        KLOG_E("device get reg error");
        goto errout;
    }

    KLOG_I("reg: %" PRIx64 ", size: 0x%" PRIxPTR, fmsh_slcr_addr.paddr, fmsh_slcr_addr.size);

    fmsh_slcr_addr.vaddr = (virt_addr_t)ioremap(fmsh_slcr_addr.paddr, fmsh_slcr_addr.size);

    /* unlock the SLCR so that registers can be changed */
    fmsh_slcr_unlock();

    slcr_is_enabled = 1;

errout:
    of_node_put(np);
    return ret;
}

static struct cpu_ops fmsh_cpu_ops = {
    .name = "fmsh",
    .init = fmsh_ops_init,
    .cpu_on = fmsh_cpu_on,
    .cpu_off = fmsh_system_poweroff,
    .cpu_suspend = fmsh_cpu_suspend,
    .cpu_migrate = fmsh_cpu_migrate,
    .cpu_reset = fmsh_cpu_reset,
    .system_poweroff = fmsh_system_poweroff,
};

int fmsh_method_init(struct cpudev *cpu)
{
    cpu->method.ops = &fmsh_cpu_ops;

    if (cpu->method.ops && cpu->method.ops->init)
        cpu->method.ops->init(cpu);

    return 0;
}
