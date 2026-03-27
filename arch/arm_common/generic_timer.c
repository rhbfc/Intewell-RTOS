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

#include <barrier.h>
#include <clock/clockchip.h>
#include <cpu.h>
#include <cpuid.h>
#include <driver/of.h>
#include <kmalloc.h>
#include <stdio.h>
#include <string.h>
#include <system/kconfig.h>
#include <time/ktime.h>
#include <ttos_pic.h>
#include <ttos_time.h>
#include <time/ktimer.h>

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...) KLOG_E(msg)
#else
#define DPRINTF(msg...)
#endif

#define KLOG_TAG "generic_timer.c"
#include <klog.h>

#define ARM_TIMER_PHYS_NONSECURE (30)
#define ARM_TIMER_PHYS_SECURE (29)

/* Generic timer system registers and CNTKCTL bits defined by the ARM timer spec. */
enum timer_sysreg_id
{
    TIMER_SYSREG_CNTFRQ,
    TIMER_SYSREG_CNTHCTL,
    TIMER_SYSREG_CNTKCTL,
    TIMER_SYSREG_CNTHP_CTL,
    TIMER_SYSREG_CNTHP_TVAL,
    TIMER_SYSREG_CNTP_CTL,
    TIMER_SYSREG_CNTP_TVAL,
    TIMER_SYSREG_CNTV_CTL,
    TIMER_SYSREG_CNTV_TVAL,
};

#define TIMER_CTL_IMASK       (1U << 1)
#define TIMER_CTL_ISTATUS     (1U << 2)

#define CNTKCTL_USR_PCTEN     (1U << 0)
#define CNTKCTL_USR_VCTEN     (1U << 1)
#define CNTKCTL_VIRT_EVT_EN   (1U << 2)
#define CNTKCTL_USR_VTEN      (1U << 8)
#define CNTKCTL_USR_PTEN      (1U << 9)

uint64_t ns_to_timer_count (uint64_t ns, uint64_t freq);

enum gen_timer_type
{
    GENERIC_SPHYSICAL_TIMER,
    GENERIC_PHYSICAL_TIMER,
};

static u32 generic_timer_hz = 0;
static u32 timer_irq[2];

static void generic_timer_get_freq(void)
{
    if (generic_timer_hz == 0)
    {
        generic_timer_hz = ttos_time_freq_get();
    }
}

int arch_timer_freq_init(void)
{
    generic_timer_get_freq();
    return 0;
}

static void generic_timer_stop(void)
{
    ttos_time_disable();
}

static void generic_timer_set_mode(enum clockchip_mode mode, struct clockchip *cc)
{
    (void)cc;

    switch (mode)
    {
    case CLOCKCHIP_MODE_UNUSED:
    case CLOCKCHIP_MODE_SHUTDOWN:
        generic_timer_stop();
        break;
    default:
        break;
    }
}

static int generic_timer_set_next_event(unsigned long long evt, struct clockchip *cc)
{
    (void)evt;

    uint64_t count = ns_to_timer_count (cc->next_event, cc->freq);
    ttos_time_count_set(count);

    return 0;
}

static inline void generic_timer_reg_write(enum timer_sysreg_id reg, u32 val)
{
    switch (reg)
    {
    case TIMER_SYSREG_CNTFRQ:
        write_cntfrq(val);
        break;
    case TIMER_SYSREG_CNTHCTL:
        write_cnthctl(val);
        break;
    case TIMER_SYSREG_CNTKCTL:
        write_cntkctl(val);
        break;
    case TIMER_SYSREG_CNTHP_CTL:
        write_cnthp_ctl(val);
        break;
    case TIMER_SYSREG_CNTHP_TVAL:
        write_cnthp_tval(val);
        break;
    case TIMER_SYSREG_CNTP_CTL:
        write_cntp_ctl(val);
        break;
    case TIMER_SYSREG_CNTP_TVAL:
        write_cntp_tval(val);
        break;
    case TIMER_SYSREG_CNTV_CTL:
        write_cntv_ctl(val);
        break;
    case TIMER_SYSREG_CNTV_TVAL:
        write_cntv_tval(val);
        break;
    default:
        KLOG_E("Trying to write invalid generic-timer register");
    }

    isb();
}

static inline u32 generic_timer_reg_read(enum timer_sysreg_id reg)
{
    u32 val;

    switch (reg)
    {
    case TIMER_SYSREG_CNTFRQ:
        val = read_cntfrq();
        break;
    case TIMER_SYSREG_CNTHCTL:
        val = read_cnthctl();
        break;
    case TIMER_SYSREG_CNTKCTL:
        val = read_cntkctl();
        break;
    case TIMER_SYSREG_CNTHP_CTL:
        val = read_cnthp_ctl();
        break;
    case TIMER_SYSREG_CNTHP_TVAL:
        val = read_cnthp_tval();
        break;
    case TIMER_SYSREG_CNTP_CTL:
        val = read_cntp_ctl();
        break;
    case TIMER_SYSREG_CNTP_TVAL:
        val = read_cntp_tval();
        break;
    case TIMER_SYSREG_CNTV_CTL:
        val = read_cntv_ctl();
        break;
    case TIMER_SYSREG_CNTV_TVAL:
        val = read_cntv_tval();
        break;
    default:
        KLOG_E("Trying to read invalid generic-timer register\n");
    }

    return val;
}

static void generic_phys_timer_handler(uint32_t irq, void *dev)
{
    u32 ctl;
    struct clockchip *cc = dev;

    if (cc->bound_on != cpuid_get())
    {
        return;
    }

    ctl = generic_timer_reg_read(TIMER_SYSREG_CNTP_CTL);
    if (!(ctl & TIMER_CTL_ISTATUS))
    {
        DPRINTF("%s: suprious interrupt\n", __func__);
        return;
    }

    ctl |= TIMER_CTL_IMASK;
    generic_timer_reg_write(TIMER_SYSREG_CNTP_CTL, ctl);

    cc->event_handler(cc);

    return;
}

static int generic_timer_startup(void)
{
    int rc;
    u32 irq;
    struct clockchip *cc;
    struct device_node *np;

    generic_timer_stop();

    cc = zalloc(sizeof(struct clockchip));
    if (!cc)
    {
        return -1;
    }

    np = of_find_compatible_node(NULL, NULL, "arm,armv7-timer");
    if (!np)
    {
        np = of_find_compatible_node(NULL, NULL, "arm,armv8-timer");
    }

    if (IS_ENABLED(CONFIG_ARM_32) &&
        of_property_read_bool(np, "arm,cpu-registers-not-fw-configured"))
    {
        cc->name = "GENERIC_SPHYSICAL_TIMER";
        cc->hw_irq = timer_irq[GENERIC_SPHYSICAL_TIMER];
    }
    else
    {
        cc->name = "GENERIC_PHYSICAL_TIMER";
        cc->hw_irq = timer_irq[GENERIC_PHYSICAL_TIMER];
    }

    cc->freq = generic_timer_hz;
    cc->set_mode = &generic_timer_set_mode;
    cc->set_next_event = &generic_timer_set_next_event;
    cc->priv = NULL;
    cc->bound_on = cpuid_get();

    irq = ttos_pic_irq_alloc(NULL, cc->hw_irq);
    /* Register irq handler for timer */
    rc = ttos_pic_irq_install(irq, ktimer_interrupt, cc, IRQ_SHARED, "PHYSICAL_TIMER");
    if (rc)
    {
        ttos_pic_irq_uninstall(irq, "PHYSICAL_TIMER");
        free(cc);
    }

    ttos_pic_irq_priority_set(irq, 0);

    ttos_pic_irq_unmask(irq);

    return 0;
}

static void arch_counter_set_user_access(void)
{
    u32 cntkctl = generic_timer_reg_read(TIMER_SYSREG_CNTKCTL);

    /* 禁止用户访问定时器的比较寄存器、控制寄存器和禁止虚拟事件流  */
    cntkctl &=
        ~(CNTKCTL_USR_PTEN | CNTKCTL_USR_VTEN | CNTKCTL_USR_VCTEN |
          CNTKCTL_VIRT_EVT_EN | CNTKCTL_USR_PCTEN);

    /* 使能用户分区可以访问频率寄存器和虚拟计数器，用户可以配置使用vtimer。*/
    cntkctl |= CNTKCTL_USR_VCTEN | CNTKCTL_USR_VTEN;

    /* 使能用户分区可以访问频率寄存器和物理计数器。*/
    cntkctl |= CNTKCTL_USR_PCTEN;

    generic_timer_reg_write(TIMER_SYSREG_CNTKCTL, cntkctl);
}

int arch_timer_clockchip_init(void)
{
    arch_counter_set_user_access();

    /* Get and Check generic timer frequency */
    generic_timer_get_freq();
    if (generic_timer_hz == 0)
    {
        return -1;
    }

    /* Get physical timer irq number */
    timer_irq[GENERIC_PHYSICAL_TIMER] = ARM_TIMER_PHYS_NONSECURE;
    timer_irq[GENERIC_SPHYSICAL_TIMER] = ARM_TIMER_PHYS_SECURE;

    generic_timer_startup();
    return 0;
}
