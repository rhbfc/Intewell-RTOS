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
 * @file： gic.h
 * @brief：
 *	    <li>gic相关宏定义类型定义</li>
 */

#ifndef _GIC_H
#define _GIC_H
/************************头文件********************************/
#include <system/types.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/************************宏定义********************************/

/* 第一个SGI中断中断号 */
#define GIC_SGI0_INTID (0)

/* 第一个PPI中断中断号 */
#define GIC_PPI0_INTID (16)

/* 第一个SPI中断中断号 */
#define GIC_SPI0_INTID (32)

#define GIC_SPURIOUS_INTID (1023)

#define GIC_IRQ_NUM_MAX (1024)

#define GIC_LPI0_INTID (8192)

/*值越大优先级越低*/
#define GIC_SGI_PRI_DEFAULT 0x90u
#define GIC_PPI_PRI_DEFAULT 0xa0u
#define GIC_SPI_PRI_DEFAULT 0xc0u
#define GIC_LPI_PRI_DEFAULT 0xc0u

#define GIC_PRI_SGI_ALL                                                        \
    ((GIC_SGI_PRI_DEFAULT << 24) | (GIC_SGI_PRI_DEFAULT << 16)                 \
     | (GIC_SGI_PRI_DEFAULT << 8) | GIC_SGI_PRI_DEFAULT)
#define GIC_PRI_PPI_ALL                                                        \
    ((GIC_PPI_PRI_DEFAULT << 24) | (GIC_PPI_PRI_DEFAULT << 16)                 \
     | (GIC_PPI_PRI_DEFAULT << 8) | GIC_PPI_PRI_DEFAULT)

#define GICD_CTLR        (0x000)
#define GICD_TYPER       (0x004)
#define GICD_IIDR        (0x008)
#define GICD_SETSPI_NSR  (0x040)
#define GICD_CLRSPI_NSR  (0x048)
#define GICD_SETSPI_SR   (0x050)
#define GICD_CLRSPI_SR   (0x058)
#define GICD_IGROUPR0    (0x080)
#define GICD_ISENABLER0  (0x100)
#define GICD_ICENABLER0  (0x180)
#define GICD_ISPENDR0    (0x200)
#define GICD_ICPENDR0    (0x280)
#define GICD_ISACTIVER0  (0x300)
#define GICD_ICACTIVER0  (0x380)
#define GICD_IPRIORITYR0 (0x400)
#define GICD_ITARGETSR0  (0x800)
#define GICD_ICFGR0      (0xC00)
#define GICD_IGRPMODR0   (0xD00)
#define GICD_NSACR       (0xE00)
#define GICD_SGIR        (0xF00)
#define GICD_CPENDSGIR0  (0xF10)
#define GICD_SPENDSGIR0  (0xF20)
#define GICD_IROUTER0    (0x6000)
#define GICD_ESTATUSR    (0xC000)
#define GICD_ERRTESTR    (0xC004)
#define GICD_SPISR0      (0xC084)

#define GICR_CTLR        (0x00)
#define GICR_IIDR        (0x04)
#define GICR_TYPER       (0x08)
#define GICR_WAKER       (0x14)
#define GICR_PROPBASER   (0x70)
#define GICR_PENDBASER   (0x78)
#define GICR_IGROUPR0    (0x80)
#define GICR_INVALLR     (0xB0)
#define GICR_SYNCR       (0xC0)
#define GICR_ISENABLER0  (0x100)
#define GICR_ICENABLER0  (0x180)
#define GICR_ICPENDR0    (0x280)
#define GICR_ICACTIVER0  (0x380)
#define GICR_ICFGR0      (0xC00)
#define GICR_ICFGR1      (0xC04)
#define GICR_IPRIORITYR0 (0x400)
#define GICR_IPRIORITYR0 (0x400)

/* 电平触发 */
#define GICD_ICFGR_TRIGGER_LEVEL (0 << 1)

/* 边沿触发 */
#define GICD_ICFGR_TRIGGER_EDGE (1 << 1)

/************************类型定义******************************/

/************************接口声明******************************/
s32 gic_pre_init (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* _GIC_H */
