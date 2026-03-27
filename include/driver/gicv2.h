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
 * @file： gicv2.h
 * @brief：
 *	    <li>gicv2相关宏定义类型定义</li>
 */

#ifndef _GICV2_H
#define _GICV2_H
/************************头文件********************************/
#include <driver/gic.h>
#include <io.h>
#include <ttos_pic.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/************************宏定义********************************/
#ifndef CONFIG_MAX_CPUS
#define CONFIG_MAX_CPUS 8
#endif

/************************GIC********************************/
#define GIC_PPI 1     /* 私有中断 */
#define GIC_SPI 0     /* 共享中断   */
#define GIC_VERSION 2 /* gic版本号 */

#define gic_dist_base(gic) (uintptr_t)(gic->gicDist.vaddr)
#define gic_cpu_base(gic) (uintptr_t)(gic->gicCpu.vaddr)

/************************GICD********************************/

/************************GICC********************************/
#define GICC_CTLR 0x0000
#define GICC_PMR 0x0004
#define GICC_BPR 0x0008
#define GICC_IAR 0x000C
#define GICC_EOIR 0x0010
#define GICC_RPR 0x0014
#define GICC_HPPIR 0x0018
#define GICC_ABPR 0x001C
#define GICC_AIAR 0x0020
#define GICC_AEOIR 0x0024
#define GICC_AHPPIR 0x0028
#define GICC_STATUSR 0x002C
#define GICC_STATUSR 0x002C
#define GICC_STATUSR 0x002C
#define GICC_STATUSR 0x002C
#define GICC_APR0 0x00D0
#define GICC_NSAPR0 0x00E0
#define GICC_IIDR 0x00FC
#define GICC_DIR 0x1000

#define PIDR2_ARCH_REV_SHIFT 4
#define PIDR2_ARCH_REV_MASK U(0xf)
#define GICD_PIDR2_GICV2 U(0xFE8)

#define GICV2_LOCK_INIT()
#define GICV2_LOCK_IRQSAVE(flag) (UNUSED_ARG(flag))
#define GICV2_UNLOCK_IRQRESTORE(flag) (UNUSED_ARG(flag))

#define GIC_INT_ALL_ENABLED 0xFF  /* 优先级 0-0xFF 可以运行 */
#define GIC_INT_ALL_DISABLED 0x00 /* 没有高于 0 的优先级，因此禁用 */
#define GIC_INT_SPURIOUS 0x3FF    /* 当前没有中断 */

/* 每个寄存器对应4个中断 */
#define GIC_INT_NUM_4_PER_REG 4

/* 每个寄存器对应8个中断 */
#define GIC_INT_NUM_8_PER_REG 8

/* 每个寄存器对应16个中断 */
#define GIC_INT_NUM_16_PER_REG 16

/* 每个寄存器对应32个中断 */
#define GIC_INT_NUM_32_PER_REG 32

#define GIC_REG_SIZE 4
#define GIC_REG_BIT 32

#define GICC_IAR_INTID_MASK (0x3FF)
#define GICC_IAR_CPUID_MASK (0x7)
#define GICC_IAR_CPUID_SHIFT (10)

#define GICC_DIS_BYPASS_MASK 0x1e0
#define GICC_ENABLE 0x1
#define GIC_CPU_ACTIVEPRIO 0xd0
#define GIC_CPU_CTRL_EOImodeNS_SHIFT 9
#define GIC_CPU_CTRL_EOImodeNS (1 << GIC_CPU_CTRL_EOImodeNS_SHIFT)

#define GIC_REG_READ32(base, reg) readl((size_t)base + reg)
#define GIC_REG_READ64(base, reg) readq((size_t)base + reg)
#define GIC_REG_WRITE32(base, reg, data) writel(data, (size_t)base + reg)
#define GIC_REG_WRITE64(base, reg, data) writeq(data, (size_t)base + reg)

/************************类型定义******************************/
typedef struct pic_gicv2
{
    struct ttos_pic pic;
    /*存储Distributor的地址信息*/
    struct devaddr_region gicDist;
    struct devaddr_region gicCpu;

    /* cpu mask */
    uint8_t affinity[CONFIG_MAX_CPUS];

    /*最大spi中断号*/
    uint32_t gic_max_irq;    /* 最大 SPI 中断号 */
    uint32_t gicPriorityLvl; /* 最低优先级 */
    void *irq_desc_list;     /* 中断描述符列表 */

} * pic_gicv2_t;

/************************外部声明******************************/

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* _GICV2_H */
