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
 * @file： gicv3.h
 * @brief：
 *	    <li>gicv3相关宏定义类型定义</li>
 */

#ifndef _GICV3_H
#define _GICV3_H
/************************头文件********************************/
#include <arch_gicv3.h>
#include <driver/devbus.h>
#include <driver/device.h>
#include <driver/gic.h>
#include <stdint.h>
#include <system/types.h>
#include <ttos_pic.h>

struct devaddr_region;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/************************宏定义********************************/
#define GICV3_DIS_BASE_SIZE 0x10000
#define GICV3_RDI_BASE_SIZE 0x10000
#define GICV3_SGI_BASE_SIZE 0x10000
#define GICV3_ITS_BASE_SIZE 0x10000

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

#define GICC_IAR_INTID_MASK (0xffffff)
#define GICC_IAR_CPUID_MASK (0x7)
#define GICC_IAR_CPUID_SHIFT (10)

#define GICR_WAKER_PS (1 << 1)
#define GICR_WAKER_CS (1 << 2)

#define GIC_IMPLEMENTER_ARM 0x43B

#define GICX_ISENABLER GICD_ISENABLER0
#define GICX_ICENABLER GICD_ICENABLER0
#define GICX_ISPENDR GICD_ISPENDR0
#define GICX_ICPENDR GICD_ICPENDR0
#define GICX_ISACTIVER GICD_ISACTIVER0
#define GICX_ICACTIVER GICD_ICACTIVER0
#define GICX_IPRIORITYR GICD_IPRIORITYR0
#define GICX_ICFGR GICD_ICFGR0

#define GICD_PIDR2 0xFFE8

#define GICD_CTLR_RWP (1UL << 31)
#define GICD_CTLR_ARE_NS (1U << 4)
#define GICD_CTLR_EnABLE_G1NS (1U << 1)
#define GICD_CTLR_ENABLE_G1 (1U << 0)
#define GICD_CTLR_ENABLE_G1A (1U << 1)

#define GICC_SRE_EL1_SRE (1UL << 0)
#define GIC_PIDR2_ARCHREV_MASK (0xf0)
#define PIDR2_GICv3 (0x30)

#define GICD_IROUTER_SPI_MODE_ANY (1UL << 31)
#define GICD_TYPE_ID_BITS_SHIFT 19

#define ICC_PMR_DEFAULT (0xff)

#define GICR_TYPER_LAST (1U << 4)

/* Macro to access the Redistributor Control Register (GICR_CTLR) */
#define GICR_CTLR_UWP (1 << 31)
#define GICR_CTLR_DPG1S (1 << 26)
#define GICR_CTLR_DPG1NS (1 << 25)
#define GICR_CTLR_DPG0 (1 << 24)
#define GICR_CTLR_RWP (1 << 3)
#define GICR_CTLR_IR (1 << 2)
#define GICR_CTLR_CES (1 << 1)
#define GICR_CTLR_EnableLPI (1 << 0)

#define GIC_REG32(base, reg) (*(volatile uint32_t *)((size_t)(base) + (reg)))
#define GIC_REG64(base, reg) (*(volatile uint64_t *)((size_t)(base) + (reg)))
#define GIC_REG_READ32(base, reg) GIC_REG32(base, reg)
#define GIC_REG_READ64(base, reg) GIC_REG64(base, reg)
#define GIC_REG_WRITE32(base, reg, data) GIC_REG32(base, reg) = (data)
#define GIC_REG_WRITE64(base, reg, data) GIC_REG64(base, reg) = (data)

#ifndef CONFIG_MAX_CPUS
#define CONFIG_MAX_CPUS 32
#endif

/************************类型定义******************************/

typedef struct pic_gicv3
{
    struct ttos_pic pic;
    struct ttos_pic *its;

    /*存储Distributor的地址信息*/
    struct devaddr_region dist;

    /*当作数组用，存储每个cpu的Redistributor的地址信息*/
    struct devaddr_region redist[CONFIG_MAX_CPUS];

    /*aff0、aff1、aff2、aff3*/
    uint32_t affinity[CONFIG_MAX_CPUS];

    /*最大spi中断号*/
    uint32_t gic_max_irq;

    void *irq_desc_list;

    /* Shared LPI tables (Redistributor side) */
    uint64_t lpi_propbases_va; /* VA of shared PROPBASER table */
    uint64_t lpi_pendbases_va; /* VA of per-CPU PENDBASER base (CPU0 start VA) */
    uint32_t lpi_prop_size;    /* bytes */
    uint32_t lpi_pend_size;    /* bytes per CPU */
    int lpi_tables_inited;     /* 0 = not inited, 1 = inited */

} * pic_gicv3_t;

/************************外部声明******************************/

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* _GICV3_H */
