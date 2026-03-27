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
#include <barrier.h>
#include <cpuid.h>
#include <driver/cpudev.h>
#include <driver/devbus.h>
#include <driver/device.h>
#include <driver/driver.h>
#include <driver/gicv2.h>
#include <driver/of.h>
#include <driver/platform.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <system/bitops.h>
#include <ttos_init.h>
#include <ttos_pic.h>

#undef KLOG_TAG
#define KLOG_TAG "GICv2"
#include <klog.h>

/************************宏 定 义******************************/
/************************类型定义******************************/
/************************外部声明******************************/
/************************前向声明******************************/
/************************模块变量******************************/
/************************全局变量******************************/
/************************实   现*******************************/

/************************外部实现start******************************/

/************************外部实现end******************************/
static inline unsigned int gic_version(uintptr_t base)
{
    unsigned int ver = GIC_REG_READ32(base, GICD_PIDR2_GICV2);
    ver = (ver >> PIDR2_ARCH_REV_SHIFT) & PIDR2_ARCH_REV_MASK;
    return ver;
}

/**
 * @brief
 *    获取affinity，在发送SGI中断时需要使用
 * @param[in] 无
 * @retval 无
 */
static void gicv2_affi_get(pic_gicv2_t gic)
{
    uint32_t cpuid = cpuid_get();
    uint32_t affinity = 0;

    /* 通过读取SGI和PPI的GICD_ITARGETSR寄存器获取cpu自身的cpumask */
    for (size_t i = 0; i < GIC_SPI0_INTID; i += 4)
    {
        affinity = GIC_REG_READ32(gic_dist_base(gic), GICD_ITARGETSR0 + i);
        affinity |= affinity >> 16;
        affinity |= affinity >> 8;
        if (affinity)
            break;
    }

    gic->affinity[cpuid] = affinity;

    KLOG_D("gicv2 cpu%d affinity:0x%x", cpuid, gic->affinity[cpuid]);
}

/**
 * @brief
 *    对gic中断配置寄存器的读操作
 * @param[in] base GICD_BASE或者GICR_BASE或者GICS_BASE
 * @param[in] offset 具体的中断配置寄存器偏移
 * @param[in] index index索引，可是irq number
 * @param[in] per_reg 每个gic寄存器可配置的中断个数
 * @retval 值
 */
static uint32_t gicv2_index_reg_read(uint64_t base, uint32_t offset, uint32_t index,
                                     uint32_t per_reg)
{
    uint32_t val = 0;
    uint32_t reg_offset = 0;
    uint32_t bit_num = GIC_REG_BIT / per_reg;

    /*计算寄存器偏移*/
    reg_offset = offset + (index / per_reg) * GIC_REG_SIZE;
    val = GIC_REG_READ32(base, reg_offset);

    /*获取val值*/
    val >>= ((index % per_reg) * bit_num);
    val &= ((1 << bit_num) - 1);

    return val;
}

/**
 * @brief
 *    对gic中断配置寄存器的写操作
 * @param[in] base GICD_BASE或者GICR_BASE或者GICS_BASE
 * @param[in] offset 具体的中断配置寄存器偏移
 * @param[in] index index索引，可是irq number
 * @param[in] data 要写入的数据
 * @param[in] per_reg 每个gic寄存器可配置的中断个数
 * @retval 无
 */
static void gicv2_index_reg_write(uint64_t base, uint32_t offset, uint32_t index, uint32_t data,
                                  uint32_t per_reg)
{
    uint32_t val = 0;
    uint32_t reg_offset = 0;
    uint32_t bit_num = GIC_REG_BIT / per_reg;
    uint32_t mask = 0;

    /*计算寄存器偏移*/
    reg_offset = offset + (index / per_reg) * GIC_REG_SIZE;
    val = GIC_REG_READ32(base, reg_offset);

    /*计算mask*/
    mask = ((1U << bit_num) - 1U) << ((index % per_reg) * bit_num);

    /*只保留data的有效位*/
    data = data & ((1U << bit_num) - 1U);
    /*组合val值*/
    val = (val & ~mask) | (data << ((index % per_reg) * bit_num));

    GIC_REG_WRITE32(base, reg_offset, val);
}

/**
 * @brief
 *    distributor初始化
 * @param[in] 无
 * @retval 无
 */
static void gicv2_distributor_init(pic_gicv2_t gic)
{
    uint32_t val;
    uint8_t affinity;

    affinity = gic->affinity[cpuid_get()];

    val = GIC_REG_READ32(gic_dist_base(gic), GICD_TYPER);

    gic->gic_max_irq = ((val & 0x1F) + 1) * 32;

    /* disable distributor */
    GIC_REG_WRITE32(gic_dist_base(gic), GICD_CTLR, 0);

    for (size_t i = GIC_SPI0_INTID; i < gic->gic_max_irq; i++)
    {
        /* 配置所有SPI默认为edge-triggered */
        gicv2_index_reg_write(gic_dist_base(gic), GICD_ICFGR0, i, GICD_ICFGR_TRIGGER_EDGE,
                              GIC_INT_NUM_16_PER_REG);
        /* 配置所有SPI默认的中断优先级 */
        gicv2_index_reg_write(gic_dist_base(gic), GICD_IPRIORITYR0, i, GIC_SPI_PRI_DEFAULT,
                              GIC_INT_NUM_4_PER_REG);
        /* 屏蔽所有SPI中断 */
        GIC_REG_WRITE32(gic_dist_base(gic),
                        GICD_ICENABLER0 + (i / GIC_INT_NUM_32_PER_REG) * GIC_REG_SIZE,
                        1 << (i % GIC_INT_NUM_32_PER_REG));
        /* 设置所有SPI中断为deactive状态 */
        GIC_REG_WRITE32(gic_dist_base(gic),
                        GICD_ICACTIVER0 + (i / GIC_INT_NUM_32_PER_REG) * GIC_REG_SIZE,
                        1 << (i % GIC_INT_NUM_32_PER_REG));
        /* 所有spi中断配置到GROUPR0 */
        // gicv2_index_reg_write(gic_dist_base(gic), GICD_IGROUPR0, i, 0, GIC_INT_NUM_32_PER_REG);
        /* 把所有spi中断都路由到bootcpu */
        gicv2_index_reg_write(gic_dist_base(gic), GICD_ITARGETSR0, i, affinity,
                              GIC_INT_NUM_4_PER_REG);
    }

    /* enable distributor */
    GIC_REG_WRITE32(gic_dist_base(gic), GICD_CTLR, 1);
}

static bool gic_check_gicv2(uintptr_t base)
{
    u32 val = GIC_REG_READ32(base, GICC_IIDR);
    return (val & 0xff0fff) == 0x02043B;
}

static void gic_cpu_if_up(pic_gicv2_t gic)
{
    uintptr_t cpu_base = gic_cpu_base(gic); // gic_data_cpu_base(gic);
    u32 bypass = 0;
    u32 mode = 0;
    int i;

    // mode = GIC_CPU_CTRL_EOImodeNS;

    if (gic_check_gicv2(cpu_base))
        for (i = 0; i < 4; i++)
            GIC_REG_WRITE32(cpu_base, GICC_APR0 + i * 4, 0);

    /*
     * Preserve bypass disable bits to be written back later
     */
    bypass = GIC_REG_READ32(cpu_base, GICC_CTLR);
    bypass &= GICC_DIS_BYPASS_MASK;

    GIC_REG_WRITE32(cpu_base, GICC_CTLR, bypass | mode | GICC_ENABLE);
}

/**
 * @brief
 *    cpu interface相关初始化
 * @param[in] 无
 * @retval 无
 */
static void gicv2_cpu_init(pic_gicv2_t gic)
{
    for (size_t i = GIC_PPI0_INTID; i < GIC_SPI0_INTID; i++)
    {
        /* 配置所有PPI默认为edge-triggered */
        gicv2_index_reg_write(gic_dist_base(gic), GICD_ICFGR0, i, GICD_ICFGR_TRIGGER_EDGE,
                              GIC_INT_NUM_16_PER_REG);
        /* 配置所有PPI默认的中断优先级 */
        gicv2_index_reg_write(gic_dist_base(gic), GICD_IPRIORITYR0, i, GIC_PPI_PRI_DEFAULT,
                              GIC_INT_NUM_4_PER_REG);
        /* 屏蔽所有PPI中断 */
        GIC_REG_WRITE32(gic_dist_base(gic),
                        GICD_ICENABLER0 + (i / GIC_INT_NUM_32_PER_REG) * GIC_REG_SIZE,
                        1 << (i % GIC_INT_NUM_32_PER_REG));
        /* 设置所有PPI中断为deactive状态 */
        GIC_REG_WRITE32(gic_dist_base(gic),
                        GICD_ICACTIVER0 + (i / GIC_INT_NUM_32_PER_REG) * GIC_REG_SIZE,
                        1 << (i % GIC_INT_NUM_32_PER_REG));
    }

    for (size_t i = 0; i < GIC_PPI0_INTID; i++)
    {
        /* 配置所有SGI默认的中断优先级 */
        gicv2_index_reg_write(gic_dist_base(gic), GICD_IPRIORITYR0, i, GIC_SGI_PRI_DEFAULT,
                              GIC_INT_NUM_4_PER_REG);
        /* 设置所有SGI中断为deactive状态 */
        GIC_REG_WRITE32(gic_dist_base(gic),
                        GICD_ICACTIVER0 + (i / GIC_INT_NUM_32_PER_REG) * GIC_REG_SIZE,
                        1 << (i % GIC_INT_NUM_32_PER_REG));
    }

    /* 关闭优先级组 */
    GIC_REG_WRITE32(gic_cpu_base(gic), GICC_BPR, 0);

    /* 设置默认的中断屏蔽优先级为255，即所有优先级中断都能触发 */
    GIC_REG_WRITE32(gic_cpu_base(gic), GICC_PMR, 0xff);

    gic_cpu_if_up(gic);
}

/**
 * @brief
 *    gicv2初始化,每个核初始化都需要通过ttos_pic_init()来调用该函数
 * @param[in] 无
 * @retval 0 初始化成功
 */
static int32_t gicv2_init(struct ttos_pic *pic)
{
    pic_gicv2_t gic = (pic_gicv2_t)pic;
    unsigned int version = 0;

    /* 获得 GIC CPU mask*/
    gicv2_affi_get(gic);

    if (is_bootcpu())
    {
        KLOG_D("gicv2 init start!");

        /* 获取版本信息 */
        version = gic_version((uintptr_t)gic_dist_base(gic));
        if (version != GIC_VERSION)
        {
            KLOG_E("ERROR:[%s,%d] gic version:0x%x\n", __FUNCTION__, __LINE__, version);
            return (-EIO);
        }

        GICV2_LOCK_INIT();

        /*distributor初始化*/
        gicv2_distributor_init(gic);

        gic->irq_desc_list = calloc(1, gic->gic_max_irq * sizeof(struct ttos_irq_desc));
    }

    /*cpu interface相关初始化*/
    gicv2_cpu_init(gic);

    if (is_bootcpu())
    {
        KLOG_D("gicv2 init end!");
    }
    else
    {
        return (-1);
    }

    return (0);
}

/**
 * @brief
 *    屏蔽指定中断
 * @param[in] irq 中断号
 * @retval 0 设置成功
 */
static int32_t gicv2_irq_mask(struct ttos_pic *pic, uint32_t irq)
{
    pic_gicv2_t gic = (pic_gicv2_t)pic;

    if (irq > gic->gic_max_irq)
    {
        KLOG_E("irq number wrong!");
        return (-EIO);
    }

    GIC_REG_WRITE32(gic_dist_base(gic),
                    GICD_ICENABLER0 + (irq / GIC_INT_NUM_32_PER_REG) * GIC_REG_SIZE,
                    1 << (irq % GIC_INT_NUM_32_PER_REG));
    return (0);
}

/**
 * @brief
 *    取消屏蔽指定中断
 * @param[in] irq 中断号
 * @retval 0 设置成功
 */
static int32_t gicv2_irq_unmask(struct ttos_pic *pic, uint32_t irq)
{
    pic_gicv2_t gic = (pic_gicv2_t)pic;

    if (irq > gic->gic_max_irq)
    {
        KLOG_E("irq number wrong!");
        return (-EIO);
    }

    GIC_REG_WRITE32(gic_dist_base(gic),
                    GICD_ISENABLER0 + (irq / GIC_INT_NUM_32_PER_REG) * GIC_REG_SIZE,
                    1 << (irq % GIC_INT_NUM_32_PER_REG));

    return (0);
}

/**
 * @brief
 *    中断确认
 * @param[in][out] src_cpu 源cpu(针对sgi)指针
 * @param[in][out] irq 产生的中断号指针
 * @retval 0 设置成功
 */
static int32_t gicv2_irq_ack(struct ttos_pic *pic, uint32_t *src_cpu, uint32_t *irq)
{
    uint32_t val = 0;
    uint32_t irqno = 0;
    pic_gicv2_t gic = (pic_gicv2_t)pic;

    if (pic == NULL)
    {
        KLOG_E("pic is NULL");
        return -EIO;
    }

    if (!src_cpu || !irq)
    {
        KLOG_E("Null pointer!");
        return (-EIO);
    }

    val = GIC_REG_READ32(gic_cpu_base(gic), GICC_IAR);

    /*
     *GICC_IAR:
     *GICC_IAR[9:0]->INTID
     *GICC_IAR[12:10]->CPUID
     */

    /* 获取中断ID */
    irqno = val & GICC_IAR_INTID_MASK;

    /* 如果获取的中断号为1023伪中断号，返回错误 */
    if (irqno == GIC_INT_SPURIOUS)
    {
        KLOG_E("int no. 1023!");
        return (-EIO);
    }

    *src_cpu = 0;

    /* 检查中断是否为 IPI */
    if (irqno < GIC_PPI0_INTID)
    {
        /* 获取cpuid */
        *src_cpu = (val >> GICC_IAR_CPUID_SHIFT) & GICC_IAR_CPUID_MASK;
    }

    irqno += pic->virt_irq_rang[0];
    *irq = irqno;

    return 0;
}

/**
 * @brief
 *    中断结束
 * @param[in] irq 中断号
 * @param[in] src_cpu 对于SGI中断，表示源cpu，其他中断则无效
 * @retval 0 设置成功
 */
static int32_t gicv2_irq_eoi(struct ttos_pic *pic, uint32_t irq, uint32_t src_cpu)
{
    pic_gicv2_t gic = (pic_gicv2_t)pic;
    uint32_t val = 0;

    val = (src_cpu & GICC_IAR_CPUID_MASK) << GICC_IAR_CPUID_SHIFT;
    val |= (irq)&GICC_IAR_INTID_MASK;

    GIC_REG_WRITE32(gic_cpu_base(gic), GICC_EOIR, val);

    return (0);
}

/**
 * @brief
 *    产生软件中断
 * @param[in] irq 中断号
 * @retval 0 设置成功
 * @retval EIO 设置失败
 */
static int32_t gicv2_irq_pending_set(struct ttos_pic *pic, uint32_t irq)
{
    pic_gicv2_t gic = (pic_gicv2_t)pic;
    size_t flags = 0;

    if (irq > gic->gic_max_irq)
    {
        KLOG_E("irq number wrong!");
        return (-EIO);
    }

    if (irq >= GIC_PPI0_INTID)
    {
        if (irq >= GIC_SPI0_INTID)
        {
            GICV2_LOCK_IRQSAVE(flags);
        }

        GIC_REG_WRITE32(gic_dist_base(gic),
                        GICD_ISPENDR0 + (irq / GIC_INT_NUM_32_PER_REG) * GIC_REG_SIZE,
                        1 << (irq % GIC_INT_NUM_32_PER_REG));

        if (irq >= GIC_SPI0_INTID)
        {
            GICV2_UNLOCK_IRQRESTORE(flags);
        }
    }
    else
    {
        GIC_REG_WRITE32(gic_dist_base(gic),
                        GICD_SPENDSGIR0 + (irq / GIC_INT_NUM_4_PER_REG) * GIC_REG_SIZE,
                        1 << (irq % GIC_INT_NUM_4_PER_REG));
    }

    return (0);
}

/**
 * @brief
 *    获取中断是否是pending状态
 * @param[in] irq 中断号
 * @param[in][out] pending 状态
 * @retval 0 成功
 * @retval EIO 失败
 */
static int32_t gicv2_irq_pending_get(struct ttos_pic *pic, uint32_t irq, bool *pending)
{
    pic_gicv2_t gic = (pic_gicv2_t)pic;
    size_t flags = 0;

    if (irq > gic->gic_max_irq || !pending)
    {
        KLOG_E("irq number wrong or null pointer!");
        return (-EIO);
    }

    if (irq >= GIC_SPI0_INTID)
    {
        GICV2_LOCK_IRQSAVE(flags);
    }

    *pending = gicv2_index_reg_read(gic_dist_base(gic), GICD_ISPENDR0, irq, GIC_INT_NUM_32_PER_REG);

    if (irq >= GIC_SPI0_INTID)
    {
        GICV2_UNLOCK_IRQRESTORE(flags);
    }

    return (0);
}

/**
 * @brief
 *    设置指定中断优先级
 * @param[in] irq 中断号
 * @param[in] priority 优先级
 * @retval 0 设置成功
 */
static int32_t gicv2_irq_priority_set(struct ttos_pic *pic, uint32_t irq, uint32_t priority)
{
    pic_gicv2_t gic = (pic_gicv2_t)pic;
    size_t flags = 0;

    if (irq > gic->gic_max_irq)
    {
        KLOG_E("irq number wrong!");
        return (-EIO);
    }

    if (irq >= GIC_SPI0_INTID)
    {
        GICV2_LOCK_IRQSAVE(flags);
    }

    gicv2_index_reg_write(gic_dist_base(gic), GICD_IPRIORITYR0, irq, priority,
                          GIC_INT_NUM_4_PER_REG);

    if (irq >= GIC_SPI0_INTID)
    {
        GICV2_UNLOCK_IRQRESTORE(flags);
    }

    return (0);
}

/**
 * @brief
 *    获取指定中断优先级
 * @param[in] irq 中断号
 * @param[in][out] priority 优先级
 * @retval 0 成功
 */
static int32_t gicv2_irq_priority_get(struct ttos_pic *pic, uint32_t irq, uint32_t *priority)
{
    pic_gicv2_t gic = (pic_gicv2_t)pic;
    size_t flags = 0;

    if (irq > gic->gic_max_irq || !priority)
    {
        KLOG_E("irq number wrong or null pointer!");
        return (-EIO);
    }

    if (irq >= GIC_SPI0_INTID)
    {
        GICV2_LOCK_IRQSAVE(flags);
    }

    *priority =
        gicv2_index_reg_read(gic_dist_base(gic), GICD_IPRIORITYR0, irq, GIC_INT_NUM_4_PER_REG);

    if (irq >= GIC_SPI0_INTID)
    {
        GICV2_UNLOCK_IRQRESTORE(flags);
    }

    return (0);
}

/**
 * @brief
 *    设置中断亲和性
 * @param[in] irq 中断号
 * @param[in] cpu cpu的id，从0开始，2，就表示设置该中断路由到第3个物理cpu
 * @retval 0 设置成功
 * @retval EIO 设置失败
 */
static int32_t gicv2_irq_affinity_set(struct ttos_pic *pic, uint32_t irq, uint32_t cpu)
{
    pic_gicv2_t gic = (pic_gicv2_t)pic;
    uint8_t affinity = 0;

    if (irq < GIC_SPI0_INTID || cpu >= cpu_numbers_get())
    {
        /*只能是SPI中断，其他中断不需要设置*/
        /*cpu需要小于CONFIG_MAX_CPUS数*/
        KLOG_E("irq number wrong or cpu number wrong!");
        return (-EIO);
    }

    affinity = gic->affinity[cpu];

    gicv2_index_reg_write(gic_dist_base(gic), GICD_ITARGETSR0, irq, affinity,
                          GIC_INT_NUM_4_PER_REG);

    return (0);
}

/**
 * @brief
 *    设置中断触发的类型
 * @param[in] irq 中断号
 * @param[in] type 类型，边沿触发，或者电平触发
 * @retval 0 设置成功
 * @retval EIO 设置失败
 */
static int32_t gicv2_irq_trigger_type_set(struct ttos_pic *pic, uint32_t irq,
                                          ttos_pic_irq_type_t type)
{
    pic_gicv2_t gic = (pic_gicv2_t)pic;
    uint32_t trigger_type = 0;
    size_t flags = 0;

    type &= (PIC_IRQ_TRIGGER_LEVEL | PIC_IRQ_TRIGGER_EDGE);

    if (irq < GIC_PPI0_INTID)
    {
        /*SGI中断只能是边沿触发，无法设置为电平触发*/
        KLOG_E("SGI interrupt can not set trigger type!");
        return (-EIO);
    }

    if (irq > gic->gic_max_irq)
    {
        KLOG_E("irq number wrong!");
        return (-EIO);
    }

    switch (type)
    {
    case PIC_IRQ_TRIGGER_LEVEL:
        trigger_type = GICD_ICFGR_TRIGGER_LEVEL;
        break;
    case PIC_IRQ_TRIGGER_EDGE:
        trigger_type = GICD_ICFGR_TRIGGER_EDGE;
        break;
    default:
        KLOG_E("set irq trigger type error!");
    }

    if (irq >= GIC_SPI0_INTID)
    {
        GICV2_LOCK_IRQSAVE(flags);
    }

    /* 设置SPI中断为边沿触发或者电平触发*/
    gicv2_index_reg_write(gic_dist_base(gic), GICD_ICFGR0, irq, trigger_type,
                          GIC_INT_NUM_16_PER_REG);

    if (irq >= GIC_SPI0_INTID)
    {
        GICV2_UNLOCK_IRQRESTORE(flags);
    }

    return (0);
}

/**
 * @brief
 *    发送SGI中断
 * @param[in] irq 中断号
 * @param[in] cpu cpu的id，从0开始，2，就表示发送给第3个物理cpu
 * @retval 0 设置成功
 * @retval EIO 设置失败
 */
static int32_t gicv2_sgi_send(struct ttos_pic *pic, uint32_t irq, uint32_t cpu, uint32_t mode)
{
    pic_gicv2_t gic = (pic_gicv2_t)pic;
    uint8_t affinity = 0;

    if (irq >= GIC_PPI0_INTID || cpu >= cpu_numbers_get())
    {
        KLOG_E("irq number wrong or cpu number wrong!\n");
        return (-EIO);
    }

    affinity = gic->affinity[cpu];

    GIC_REG_WRITE32(gic_dist_base(gic), GICD_SGIR, (affinity << 16 | irq));

    isb();

    return (0);
}

/**
 * @brief
 *    获取最大中断号
 * @param[in][out] irq 最大中断号
 * @retval 0 成功
 * @retval EIO 失败
 */
static int32_t gicv2_max_number_get(struct ttos_pic *pic, uint32_t *max_number)
{
    pic_gicv2_t gic = (pic_gicv2_t)pic;

    *max_number = gic->gic_max_irq;
    return (0);
}

static int32_t gicv2_dtb_irq_parse(struct ttos_pic *pic, uint32_t *value_table)
{
    int32_t irq_num;

    switch (value_table[0])
    {
    case GIC_SPI:
        irq_num = 32 + value_table[1];
        break;

    case GIC_PPI:
        irq_num = value_table[1];
        break;

    default:
        irq_num = -ENOENT;
        break;
    }
    return irq_num;
}

static void *gicv2_irq_desc_get(struct ttos_pic *pic, int32_t hwirq)
{
    pic_gicv2_t gic = (pic_gicv2_t)pic;
    struct ttos_irq_desc *desc_list;

    desc_list = gic->irq_desc_list;
    if (hwirq < gic->gic_max_irq)
    {
        desc_list += hwirq;
    }
    else
    {
        /* TODO: MSI */
    }

    return desc_list;
}

/* pic 通用操作 */
static struct ttos_pic_ops gicv2_ops = {
    .pic_init = gicv2_init,
    .pic_mask = gicv2_irq_mask,
    .pic_unmask = gicv2_irq_unmask,
    .pic_ack = gicv2_irq_ack,
    .pic_eoi = gicv2_irq_eoi,
    .pic_pending_set = gicv2_irq_pending_set,
    .pic_pending_get = gicv2_irq_pending_get,
    .pic_priority_set = gicv2_irq_priority_set,
    .pic_priority_get = gicv2_irq_priority_get,
    .pic_affinity_set = gicv2_irq_affinity_set,
    .pic_trigger_type_set = gicv2_irq_trigger_type_set,
    .pic_ipi_send = gicv2_sgi_send,
    .pic_max_number_get = gicv2_max_number_get,
    .pic_msi_alloc = NULL,
    .pic_msi_free = NULL,
    .pic_msi_send = NULL,
    .pic_dtb_irq_parse = gicv2_dtb_irq_parse,
    .pic_irq_desc_get = gicv2_irq_desc_get,
};

static int32_t gicv2_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct pic_gicv2 *gic;
    KLOG_I("gicv2_probe %s", dev->name);

    if (!of_property_read_bool(dev->of_node, "interrupt-controller"))
    {
        return -EIO;
    }

    gic = memalign(sizeof(phys_addr_t), sizeof(struct pic_gicv2));

    if (!gic)
    {
        return -ENOMEM;
    }

    memset(gic, 0, sizeof(struct pic_gicv2));

    of_device_get_resource_regs(dev, &gic->gicDist, 2);

    if (!gic_dist_base(gic) || !gic_cpu_base(gic))
    {
        return -EIO;
    }

    gic->pic.name = dev->driver->name;
    gic->pic.pic_ops = &gicv2_ops;
    gic->pic.flag = PIC_FLAG_CPU;
    dev->priv = (void *)gic;
    ttos_pic_register(&gic->pic);
    return 0;
}

static struct of_device_id gicv2_table[] = {
    {.compatible = "arm,gic-400"},
    {.compatible = "arm,cortex-a15-gic"},
    {.compatible = "arm,cortex-a7-gic"},
    {/* end of list */},
};

static struct platform_driver gicv2_driver = {
    .probe = gicv2_probe,
    .driver =
        {
            .name = "gic-400",
            .of_match_table = gicv2_table,
        },
};

static int32_t gicv2_driver_init(void)
{
    return platform_add_driver(&gicv2_driver);
}

INIT_EXPORT_PRE_PIC(gicv2_driver_init, "gicv2 driver init");
