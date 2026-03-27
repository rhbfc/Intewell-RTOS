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
#include <driver/gicv3.h>
#include <driver/of.h>
#include <driver/platform.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <system/bitops.h>
#include <ttos_init.h>
#include <ttos_pic.h>

#define KLOG_LEVEL KLOG_WARN
#define KLOG_TAG "GICv3"
#include <klog.h>

/************************宏 定 义******************************/

#define GICV3_LOCK_INIT()
#define GICV3_LOCK_IRQSAVE(flag) (UNUSED_ARG(flag))
#define GICV3_UNLOCK_IRQRESTORE(flag) (UNUSED_ARG(flag))

#define GICR_REG_SIZE 0x20000
#define GICR_PPI_SGI_OFFSET 0x10000
#define GICRn_LPI_BASE(base, n) (base + (n * GICR_REG_SIZE))
#define GICRn_PPI_SGI_BASE(base, n) (base + (n * GICR_REG_SIZE) + GICR_PPI_SGI_OFFSET)

#define GIC_PPI 1
#define GIC_SPI 0

/************************类型定义******************************/
/************************外部声明******************************/
/************************前向声明******************************/
static int32_t gicv3_irq_affinity_set(struct ttos_pic *pic, uint32_t irq, uint32_t cpu);
/************************模块变量******************************/
/************************全局变量******************************/
/************************实   现*******************************/

/************************外部实现start******************************/

/************************外部实现end******************************/

/**
 * @brief
 *    使能SRE,设置ICC_SRE_EL1.SRE,之后才可以在EL1访问ICC_*寄存器
 * @param[in] 无
 * @retval 无
 */
static void gicv3_sre_set(void)
{
    uint64_t val = 0;

    val = arch_gicv3_sre_read();
    val |= GICC_SRE_EL1_SRE;

    arch_gicv3_sre_write(val);
    isb();
}

/**
 * @brief
 *    探测是否支持distributor
 * @param[in] 无
 * @retval 0 probe成功
 * @retval EIO probe失败
 */
static int32_t gicv3_distributor_probe(pic_gicv3_t gic)
{
    uint32_t val = 0;

    val = GIC_REG_READ32(gic->dist.vaddr, GICD_IIDR);
    if (GIC_IMPLEMENTER_ARM != (val & 0xFFF))
    {
        KLOG_E("gicv3_distributor_probe fail");
        return (-EIO);
    }

    return (0);
}

/**
 * @brief
 *    探测是否支持redistributor
 * @param[in] index 指示哪一个cpu的redistributor
 * @retval 0 probe成功
 * @retval EIO probe失败
 */
static int32_t gicv3_redistributor_probe(pic_gicv3_t gic, int32_t index)
{
    uint32_t val;

    val = GIC_REG_READ32(gic->redist[index].vaddr, GICR_IIDR);
    if ((val & 0xFFF) != GIC_IMPLEMENTER_ARM)
    {
        KLOG_E("invalid GIC implementer");
        return (-EIO);
    }

    return (0);
}

/**
 * @brief
 *    探测是否支持distributor、redistributor
 * @retval 0 probe成功
 * @retval EIO probe失败
 */
static int32_t gicv3_dist_redist_probe(pic_gicv3_t gic)
{
    int32_t i = 0, ret = 0;

    ret |= gicv3_distributor_probe(gic);

    for (i = 0; i < cpu_numbers_get(); i++)
    {
        ret |= gicv3_redistributor_probe(gic, i);
    }

    return ret;
}

/**
 * @brief
 *    gicd gicr 基地址初始化
 * @param[in] 无
 * @retval 无
 */
static void gicv3_base_info_init(pic_gicv3_t gic)
{
    uint64_t val;
    uint32_t affinity;
    uint32_t i;
    uint32_t gic_cpus;
    virt_addr_t redist_vaddr;
    phys_addr_t redist_paddr;
    struct cpudev *cpu;

    /* 获取 gic_max_irq */
    val = GIC_REG_READ32(gic->dist.vaddr, GICD_TYPER);
    gic->gic_max_irq = ((val & 0x1F) + 1) * 32;
    gic_cpus = ((val >> 5) & 0x7) + 1;
    gic_cpus = gic_cpus >= cpu_numbers_get() ? gic_cpus : cpu_numbers_get();

    redist_vaddr = gic->redist[0].vaddr;
    redist_paddr = gic->redist[0].paddr;

    for (i = 0; i < gic_cpus; i++)
    {
        for_each_present_cpu(cpu)
        {
            affinity = GIC_REG_READ64(GICRn_LPI_BASE(redist_vaddr, i), GICR_TYPER) >> 32;

            if (affinity == gic->affinity[cpu->index])
            {
                gic->redist[cpu->index].vaddr = GICRn_LPI_BASE(redist_vaddr, i);
                gic->redist[cpu->index].paddr = GICRn_LPI_BASE(redist_paddr, i);
                gic->redist[cpu->index].size = GICR_REG_SIZE;
                break;
            }
        }

        KLOG_D("Redistributor [%d],paddr:0x%" PRIx64 " vaddr:0x%lx size:0x%" PRIXPTR, i,
               gic->redist[i].paddr, gic->redist[i].vaddr, gic->redist[i].size);
    }
}

/**
 * @brief
 *    检测gic版本
 * @param[in] 无
 * @retval EIO 失败
 * @retval 0 成功
 */
static int32_t gicv3_version_check(pic_gicv3_t gic)
{
    uint32_t val;

    val = GIC_REG_READ32(gic->dist.vaddr, GICD_PIDR2);
    val &= GIC_PIDR2_ARCHREV_MASK;

    if (val != PIDR2_GICv3)
    {
        KLOG_E("version check failed,not gicv3");
        return (-EIO);
    }

    return (0);
}

/**
 * @brief
 *    获取affinity，在发送SGI中断时需要使用
 * @param[in] 无
 * @retval 无
 */
static void gicv3_affi_get(pic_gicv3_t gic)
{
    struct cpudev *cpu;
    uint64_t affinity = 0;

    for_each_present_cpu(cpu)
    {
        affinity = cpu_hw_id_get(cpu);
        gic->affinity[cpu->index] = ((affinity >> 8) & 0xFF000000) | (affinity & 0x00FFFFFF);

        KLOG_D("gicv3 aff[0-3]{0x%x, 0x%x, 0x%x, 0x%x}", gic->affinity[cpu->index] & 0xFF,
               (gic->affinity[cpu->index] >> 8) & 0xFF, (gic->affinity[cpu->index] >> 16) & 0xFF,
               (gic->affinity[cpu->index] >> 24) & 0xFF);
    }
}

/**
 * @brief
 *    等待distributor写完成或者redistributor写操作完全通知到distributor
 * @param[in] is_dist
 * 为true则等待distributor写完成，为false则等待redistributor写操作完全通知到distributor
 * @retval 无
 */
static void gicv3_write_wait(pic_gicv3_t gic, bool is_dist)
{
    uint32_t val;
    int32_t wait_cnt = 1000;

    while (wait_cnt)
    {
        if (is_dist)
        {
            val = GIC_REG_READ32(gic->dist.vaddr, GICD_CTLR);
        }
        else
        {
            val = GIC_REG_READ32(gic->dist.vaddr, GICR_CTLR);
        }

        /* RWP, bit [31],为0表示写完成 */
        if ((val & GICD_CTLR_RWP) == 0)
        {
            break;
        }

        wait_cnt--;
    }

    if (wait_cnt == 0)
    {
        KLOG_E("gicv3_write_wait timerout!");
    }
}

/**
 * @brief
 *    等待写完成
 * @param[in] irq 中断号
 * @retval 无
 */
static void gicv3_wp_wait(pic_gicv3_t gic, uint32_t irq)
{
    if (irq < GIC_SPI0_INTID)
    {
        gicv3_write_wait(gic, false);
    }
    else
    {
        gicv3_write_wait(gic, true);
    }
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
static uint32_t gicv3_index_reg_read(uint64_t base, uint32_t offset, uint32_t index,
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
static void gicv3_index_reg_write(uint64_t base, uint32_t offset, uint32_t index, uint32_t data,
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
static void gicv3_distributor_init(struct ttos_pic *pic)
{
    uint32_t offset = 0;
    uint32_t i = 0;
    uint32_t spi_max;
    pic_gicv3_t gic = (pic_gicv3_t)pic;

    spi_max = gic->gic_max_irq;

    /* 配置所有SPI默认为edge-triggered */
    for (i = GIC_SPI0_INTID; i < spi_max; i++)
    {
        gicv3_index_reg_write(gic->dist.vaddr, GICD_ICFGR0, i, GICD_ICFGR_TRIGGER_LEVEL,
                              GIC_INT_NUM_16_PER_REG);
    }

    /* 配置SPI默认的中断优先级 */
    for (i = GIC_SPI0_INTID; i < spi_max; i++)
    {
        gicv3_index_reg_write(gic->dist.vaddr, GICD_IPRIORITYR0, i, GIC_SPI_PRI_DEFAULT,
                              GIC_INT_NUM_4_PER_REG);
    }

    /* 配置SPI默认的中断为关闭状态及deactive状态 */
    for (i = GIC_SPI0_INTID; i < spi_max; i += GIC_INT_NUM_32_PER_REG)
    {
        offset = GICD_ICENABLER0 + (i / GIC_INT_NUM_32_PER_REG) * GIC_REG_SIZE;
        GIC_REG_WRITE32(gic->dist.vaddr, offset, GENMASK(31, 0));

        offset = GICD_ICACTIVER0 + (i / GIC_INT_NUM_32_PER_REG) * GIC_REG_SIZE;
        GIC_REG_WRITE32(gic->dist.vaddr, offset, GENMASK(31, 0));
    }

    /* 配置SPI为 非安全-G1 */
    for (i = GIC_SPI0_INTID; i < spi_max; i += 32)
    {
        GIC_REG_WRITE32(gic->dist.vaddr, GICD_IGROUPR0 + (i / 32) * 4, GENMASK(31, 0));
    }

    gicv3_write_wait(gic, true);

    /* 使能Non-secure Group 1中断，在non secure状态只能访问有限的bit位*/
    GIC_REG_WRITE32(gic->dist.vaddr, GICD_CTLR, GICD_CTLR_ARE_NS | GICD_CTLR_ENABLE_G1A
                    /* | GICD_CTLR_ENABLE_G1 */);

    /* 路由所有中断到当前CPU */
    for (i = GIC_SPI0_INTID; i < spi_max; i++)
    {
        gicv3_irq_affinity_set(pic, i, 0);
    }
}

/**
 * @brief
 *    redistributor唤醒PE和cpu interface，因为在复位时，其将PE视为睡眠状态
 * @param[in] 无
 * @retval 0 成功
 * @retval EIO 失败
 */
static int32_t gicv3_cpu_awake(pic_gicv3_t gic)
{
    int32_t i, wait_cnt = 1000;
    uint32_t val = 0;

    val = GIC_REG_READ32(gic->redist[cpuid_get()].vaddr, GICR_WAKER);
    val &= ~(GICR_WAKER_PS);
    GIC_REG_WRITE32(gic->redist[cpuid_get()].vaddr, GICR_WAKER, val);

    /* 等待interface active */
    for (i = 0; i < wait_cnt; i++)
    {
        val = GIC_REG_READ32(gic->redist[cpuid_get()].vaddr, GICR_WAKER);
        if ((val & GICR_WAKER_CS) == 0)
        {
            break;
        }
    }

    if (wait_cnt == i)
    {
        KLOG_E("Disable redistributor awake cpu failed!");
        return (-EIO);
    }

    return (0);
}

/**
 * @brief
 *    redistributor初始化
 * @param[in] 无
 * @retval 无
 */
static void gicv3_redistributor_init(pic_gicv3_t gic)
{
    int32_t i = 0;
    uint64_t val = 0;
    virt_addr_t base;

    val = GIC_REG_READ64(gic->redist[cpuid_get()].vaddr, GICR_TYPER);
    if ((val >> 32) != gic->affinity[cpuid_get()])
    {
        KLOG_E("affinity no match. GICR_TYPER: aff: 0x%" PRIx64 ", "
               "gic->affinity: 0x%" PRIx32,
               val >> 32, gic->affinity[cpuid_get()]);
    }

    base = gic->redist[cpuid_get()].vaddr + GICR_PPI_SGI_OFFSET;

    /* redistributor唤醒PE和cpu interface */
    gicv3_cpu_awake(gic);

    /* 设置SGI中断优先级 */
    for (i = 0; i < GIC_PPI0_INTID; i += GIC_INT_NUM_4_PER_REG)
    {
        GIC_REG_WRITE32(base, GICR_IPRIORITYR0 + (i / 4) * 4, GIC_PRI_SGI_ALL);
    }

    /* 设置PPI中断优先级 */
    for (i = GIC_PPI0_INTID; i < GIC_SPI0_INTID; i += GIC_INT_NUM_4_PER_REG)
    {
        GIC_REG_WRITE32(base, GICR_IPRIORITYR0 + (i / 4) * 4, GIC_PRI_PPI_ALL);
    }

    /* 设置所有PPI中断默认为level-triggered*/
    for (i = GIC_PPI0_INTID; i < GIC_SPI0_INTID; i++)
    {
        gicv3_index_reg_write(base, GICR_ICFGR1, i - GIC_PPI0_INTID, GICD_ICFGR_TRIGGER_LEVEL,
                              GIC_INT_NUM_16_PER_REG);
    }

    /* deactivate SGI PPI中断 */
    GIC_REG_WRITE32(base, GICR_ICACTIVER0, GENMASK(31, 0));

    /* 关闭PPI */
    GIC_REG_WRITE32(base, GICR_ICENABLER0, GENMASK(31, 16));
    /* 使能SGI */
    GIC_REG_WRITE32(base, GICR_ISENABLER0, GENMASK(15, 0));
    /* 配置 SGIs/PPIs 为非安全组1中断 */
    GIC_REG_WRITE32(base, GICR_IGROUPR0, GENMASK(31, 0));

    gicv3_write_wait(gic, false);
}

/**
 * @brief
 *    cpu interface、redistributor相关初始化
 * @param[in] 无
 * @retval 无
 */
static void gicv3_cpu_init(pic_gicv3_t gic)
{
    gicv3_redistributor_init(gic);

    /* 关闭优先级组 */
    arch_gicv3_bpr1_write(0);

    /* 设置默认的中断屏蔽优先级为255，即所有优先级中断都能触发 */
    arch_gicv3_pmr_write(ICC_PMR_DEFAULT);

    /* 对当前secure状态使能Group 1中断 */
    arch_gicv3_igrpen1_write(1);

    isb();
}

/**
 * @brief
 *    根据中断号获取对应的gic base
 * @param[in] irq 中断号
 * @retval gic base
 */
static uint64_t gicv3_gicx_base_get(pic_gicv3_t gic, uint32_t irq)
{
    uint64_t base = 0;

    if (irq < GIC_SPI0_INTID)
    {
        base = gic->redist[cpuid_get()].vaddr + GICR_PPI_SGI_OFFSET;
    }
    else
    {
        base = gic->dist.vaddr;
    }

    return base;
}
/**
 * @brief
 *    gicv3初始化,每个核初始化都需要通过ttos_pic_init()来调用该函数
 * @param[in] 无
 * @retval 0 初始化成功
 */
static int32_t gicv3_init(struct ttos_pic *pic)
{
    pic_gicv3_t gic = (pic_gicv3_t)pic;

    /*
     * 使能SRE,设置ICC_SRE_EL1.SRE,之后才可以在EL1访问ICC_*寄存器
     */
    gicv3_sre_set();

    if (is_bootcpu())
    {
        KLOG_D("gicv3 init start!");

        gicv3_affi_get(gic);

        /* 获取基地址 */
        gicv3_base_info_init(gic);

        /* 探测是否支持distributor、redistributor,检测gic版本 */
        if (gicv3_dist_redist_probe(gic) || gicv3_version_check(gic))
        {
            return (-EIO);
        }

        GICV3_LOCK_INIT();

        /*distributor初始化*/
        gicv3_distributor_init(pic);

        gic->irq_desc_list = calloc(1, gic->gic_max_irq * sizeof(struct ttos_irq_desc));
    }

    /*cpu interface、redistributor相关初始化*/
    gicv3_cpu_init(gic);

    if (is_bootcpu())
    {
        KLOG_D("gicv3 init end!");
    }
    else
        return (-1);

    return (0);
}

/**
 * @brief
 *    取消屏蔽指定中断
 * @param[in] irq 中断号
 * @retval 0 设置成功
 */
static int32_t gicv3_irq_unmask(struct ttos_pic *pic, uint32_t irq)
{
    pic_gicv3_t gic = (pic_gicv3_t)pic;
    uint64_t base = gicv3_gicx_base_get(gic, irq);
    size_t flags = 0;

    if (irq > gic->gic_max_irq)
    {
        KLOG_E("irq number wrong!");
        return (-EIO);
    }

    if (irq >= GIC_SPI0_INTID)
    {
        GICV3_LOCK_IRQSAVE(flags);
    }

    GIC_REG_WRITE32(base, GICX_ISENABLER + (irq / GIC_INT_NUM_32_PER_REG) * GIC_REG_SIZE,
                    1 << (irq % GIC_INT_NUM_32_PER_REG));

    if (irq >= GIC_SPI0_INTID)
    {
        GICV3_UNLOCK_IRQRESTORE(flags);
    }

    gicv3_wp_wait(gic, irq);

    return (0);
}

/**
 * @brief
 *    屏蔽指定中断
 * @param[in] irq 中断号
 * @retval 0 设置成功
 */
static int32_t gicv3_irq_mask(struct ttos_pic *pic, uint32_t irq)
{
    pic_gicv3_t gic = (pic_gicv3_t)pic;
    uint64_t base = gicv3_gicx_base_get(gic, irq);
    size_t flags = 0;

    if (irq > gic->gic_max_irq)
    {
        KLOG_E("irq number wrong!");
        return (-EIO);
    }

    if (irq >= GIC_SPI0_INTID)
    {
        GICV3_LOCK_IRQSAVE(flags);
    }

    GIC_REG_WRITE32(base, GICX_ICENABLER + (irq / GIC_INT_NUM_32_PER_REG) * GIC_REG_SIZE,
                    1 << (irq % GIC_INT_NUM_32_PER_REG));

    if (irq >= GIC_SPI0_INTID)
    {
        GICV3_UNLOCK_IRQRESTORE(flags);
    }

    gicv3_wp_wait(gic, irq);

    return (0);
}

/**
 * @brief
 *    中断确认
 * @param[in][out] src_cpu 源cpu(针对sgi)指针
 * @param[in][out] irq 产生的中断号指针
 * @retval 0 设置成功
 */
static int32_t gicv3_irq_ack(struct ttos_pic *pic, uint32_t *src_cpu, uint32_t *irq)
{
    uint32_t val = 0;
    uint32_t irqno = 0;
    pic_gicv3_t gic = (pic_gicv3_t)pic;

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

    /* 读取中断ID，确认Group 1 interrupt */
    val = arch_gicv3_iar1_read();

    /*
     *GICC_IAR:
     *GICC_IAR[9:0]->INTID
     *GICC_IAR[12:10]->CPUID
     */

    /* 获取中断ID */
    irqno = val & GICC_IAR_INTID_MASK;

    if (irqno == 1023)
    {
        return -1;
    }

    *src_cpu = 0;

    if (irqno < GIC_PPI0_INTID)
    {
        /* 针对SGI,获取发送者的cpuid */
        *src_cpu = (val >> GICC_IAR_CPUID_SHIFT) & GICC_IAR_CPUID_MASK;
    }

    if (irqno >= 8192)
    {
        uint32_t lpi_index = irqno - 8192;
        if (gic->its)
        {
            /* Multi-ITS: prefer owner by LPI index if present */
            extern struct ttos_pic *gicv3_its_pic_by_lpi_index(uint32_t lpi_index);
            struct ttos_pic *its_pic = gicv3_its_pic_by_lpi_index(lpi_index);
            struct ttos_pic *fallback = gic->its;
            pic = its_pic ? its_pic : fallback;
        }
        else
        {
            KLOG_E("ITS NODEV!");
            return (-ENODEV);
        }
        irqno = lpi_index; /* normalize for virt range add below */
    }

    irqno += pic->virt_irq_rang[0];
    *irq = irqno;

    return (0);
}

/**
 * @brief
 *    中断结束
 * @param[in] irq 中断号
 * @param[in] src_cpu 对于SGI中断，表示源cpu，其他中断则无效
 * @retval 0 设置成功
 */
static int32_t gicv3_irq_eoi(struct ttos_pic *pic, uint32_t irq, uint32_t src_cpu)
{
    uint32_t val = 0;

    val = (src_cpu & GICC_IAR_CPUID_MASK) << GICC_IAR_CPUID_SHIFT;
    val |= (irq)&GICC_IAR_INTID_MASK;

    arch_gicv3_eoir1_write(val);

    return (0);
}

/**
 * @brief
 *    产生软件中断
 * @param[in] irq 中断号
 * @retval 0 设置成功
 * @retval EIO 设置失败
 */
static int32_t gicv3_irq_pending_set(struct ttos_pic *pic, uint32_t irq)
{
    pic_gicv3_t gic = (pic_gicv3_t)pic;
    uint64_t base = gicv3_gicx_base_get(gic, irq);
    size_t flags = 0;

    if (irq > gic->gic_max_irq)
    {
        KLOG_E("irq number wrong!");
        return (-EIO);
    }

    if (irq >= GIC_SPI0_INTID)
    {
        GICV3_LOCK_IRQSAVE(flags);
    }

    GIC_REG_WRITE32(base, GICX_ISPENDR + (irq / GIC_INT_NUM_32_PER_REG) * GIC_REG_SIZE,
                    1 << (irq % GIC_INT_NUM_32_PER_REG));

    if (irq >= GIC_SPI0_INTID)
    {
        GICV3_UNLOCK_IRQRESTORE(flags);
    }

    gicv3_wp_wait(gic, irq);

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
static int32_t gicv3_irq_pending_get(struct ttos_pic *pic, uint32_t irq, bool *pending)
{
    pic_gicv3_t gic = (pic_gicv3_t)pic;
    uint64_t base = gicv3_gicx_base_get(gic, irq);
    size_t flags = 0;

    if (irq > gic->gic_max_irq || !pending)
    {
        KLOG_E("irq number wrong or null pointer!");
        return (-EIO);
    }

    if (irq >= GIC_SPI0_INTID)
    {
        GICV3_LOCK_IRQSAVE(flags);
    }

    *pending = gicv3_index_reg_read(base, GICX_ISPENDR, irq, GIC_INT_NUM_32_PER_REG);

    if (irq >= GIC_SPI0_INTID)
    {
        GICV3_UNLOCK_IRQRESTORE(flags);
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
static int32_t gicv3_irq_priority_set(struct ttos_pic *pic, uint32_t irq, uint32_t priority)
{
    pic_gicv3_t gic = (pic_gicv3_t)pic;
    uint64_t base = gicv3_gicx_base_get(gic, irq);
    size_t flags = 0;

    if (irq > gic->gic_max_irq)
    {
        KLOG_E("irq number wrong!");
        return (-EIO);
    }

    if (irq >= GIC_SPI0_INTID)
    {
        GICV3_LOCK_IRQSAVE(flags);
    }

    gicv3_index_reg_write(base, GICX_IPRIORITYR, irq, priority, GIC_INT_NUM_4_PER_REG);

    if (irq >= GIC_SPI0_INTID)
    {
        GICV3_UNLOCK_IRQRESTORE(flags);
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
static int32_t gicv3_irq_priority_get(struct ttos_pic *pic, uint32_t irq, uint32_t *priority)
{
    pic_gicv3_t gic = (pic_gicv3_t)pic;
    uint64_t base = gicv3_gicx_base_get(gic, irq);
    size_t flags = 0;

    if (irq > gic->gic_max_irq || !priority)
    {
        KLOG_E("irq number wrong or null pointer!");
        return (-EIO);
    }

    if (irq >= GIC_SPI0_INTID)
    {
        GICV3_LOCK_IRQSAVE(flags);
    }

    *priority = gicv3_index_reg_read(base, GICX_IPRIORITYR, irq, GIC_INT_NUM_4_PER_REG);

    if (irq >= GIC_SPI0_INTID)
    {
        GICV3_UNLOCK_IRQRESTORE(flags);
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
static int32_t gicv3_irq_affinity_set(struct ttos_pic *pic, uint32_t irq, uint32_t cpu)
{
    pic_gicv3_t gic = (pic_gicv3_t)pic;
    uint64_t affinity = 0;
    uint64_t aff3 = 0;

    if (irq < GIC_SPI0_INTID || cpu >= cpu_numbers_get())
    {
        /*只能是SPI中断，其他中断不需要设置*/
        /*cpu需要小于CONFIG_MAX_CPUS数*/
        KLOG_E("irq number wrong or cpu number wrong!");
        return (-EIO);
    }

    affinity = gic->affinity[cpu];

    /*将aff3按照GICD_IROUTER寄存器进行配置*/
    aff3 = (affinity >> 24) & 0xFF;
    affinity &= ~(0xFFULL << 24);
    affinity |= (aff3 << 32);

    /* 关闭SPI中断可路由到任意cpu模式 */
    affinity &= ~GICD_IROUTER_SPI_MODE_ANY;

    GIC_REG_WRITE64(gic->dist.vaddr, GICD_IROUTER0 + irq * 8, affinity);

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
static int32_t gicv3_irq_trigger_type_set(struct ttos_pic *pic, uint32_t irq,
                                          ttos_pic_irq_type_t type)
{
    pic_gicv3_t gic = (pic_gicv3_t)pic;
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
        GICV3_LOCK_IRQSAVE(flags);
        /* 设置SPI中断为边沿触发或者电平触发*/
        gicv3_index_reg_write(gic->dist.vaddr, GICD_ICFGR0, irq, trigger_type,
                              GIC_INT_NUM_16_PER_REG);
        GICV3_UNLOCK_IRQRESTORE(flags);
    }
    else
    {
        /* 设置PPI中断为边沿触发或者电平触发 */
        gicv3_index_reg_write(gic->redist[cpuid_get()].vaddr + GICR_PPI_SGI_OFFSET, GICR_ICFGR1,
                              irq - GIC_PPI0_INTID, trigger_type, GIC_INT_NUM_16_PER_REG);
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
static int32_t gicv3_sgi_send(struct ttos_pic *pic, uint32_t irq, uint32_t cpu, uint32_t mode)
{
    pic_gicv3_t gic = (pic_gicv3_t)pic;
    uint64_t val = 0;
    uint64_t aff0, aff1, aff2, aff3;
    u16 target_list;

    if (irq >= GIC_PPI0_INTID || cpu >= cpu_numbers_get())
    {
        /*中断号只能是0-15*/
        /*cpu需要小于CONFIG_MAX_CPUS数*/
        KLOG_E("irq number wrong or cpu number wrong!");
        return (-EIO);
    }

    aff0 = gic->affinity[cpu] & 0xFF;
    aff1 = (gic->affinity[cpu] >> 8) & 0xFF;
    aff2 = (gic->affinity[cpu] >> 16) & 0xFF;
    aff3 = (gic->affinity[cpu] >> 24) & 0xFF;
    target_list = 1 << aff0;

    /*组合affi*/
    val = (aff3 << 48) | (aff2 << 32) | (aff1 << 16) | target_list;
    val |= irq << 24;

    arch_gicv3_sgi1r_write(val);
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
static int32_t gicv3_max_number_get(struct ttos_pic *pic, uint32_t *max_number)
{
    pic_gicv3_t gic = (pic_gicv3_t)pic;

    *max_number = gic->gic_max_irq;
    return (0);
}

static int32_t gicv3_dtb_irq_parse(struct ttos_pic *pic, uint32_t *value_table)
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

static void *gicv3_irq_desc_get(struct ttos_pic *pic, int32_t hwirq)
{
    pic_gicv3_t gic = (pic_gicv3_t)pic;
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

/* pic通用操作 */
static struct ttos_pic_ops gicv3_ops = {
    .pic_init = gicv3_init,
    .pic_mask = gicv3_irq_mask,
    .pic_unmask = gicv3_irq_unmask,
    .pic_ack = gicv3_irq_ack,
    .pic_eoi = gicv3_irq_eoi,
    .pic_pending_set = gicv3_irq_pending_set,
    .pic_pending_get = gicv3_irq_pending_get,
    .pic_priority_set = gicv3_irq_priority_set,
    .pic_priority_get = gicv3_irq_priority_get,
    .pic_affinity_set = gicv3_irq_affinity_set,
    .pic_trigger_type_set = gicv3_irq_trigger_type_set,
    .pic_ipi_send = gicv3_sgi_send,
    .pic_max_number_get = gicv3_max_number_get,
    .pic_msi_alloc = NULL,
    .pic_msi_free = NULL,
    .pic_msi_send = NULL,
    .pic_dtb_irq_parse = gicv3_dtb_irq_parse,
    .pic_irq_desc_get = gicv3_irq_desc_get,
};

static int32_t gicv3_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct pic_gicv3 *gic;
    KLOG_I("gicv3_probe %s", dev->name);

    if (!of_property_read_bool(dev->of_node, "interrupt-controller"))
    {
        return -EIO;
    }

    gic = memalign(sizeof(phys_addr_t), sizeof(struct pic_gicv3));

    if (!gic)
    {
        return -ENOMEM;
    }

    memset(gic, 0, sizeof(struct pic_gicv3));

    of_device_get_resource_regs(dev, &gic->dist, 2);

    if (!gic->dist.vaddr && !gic->redist[0].vaddr)
    {
        return -EIO;
    }

    gic->pic.name = dev->driver->name;
    gic->pic.pic_ops = &gicv3_ops;
    gic->pic.flag = PIC_FLAG_CPU;
    dev->priv = (void *)gic;

    ttos_pic_register(&gic->pic);
    return 0;
}

static struct of_device_id gicv3_table[] = {
    {.compatible = "arm,gic-v3"},
    // { .compatible = "arm,gic-v3-its" },
    {/* end of list */},
};

static struct platform_driver gicv3_driver = {
    .probe = gicv3_probe,
    .driver =
        {
            .name = "gicv3",
            .of_match_table = gicv3_table,
        },
};

static int32_t gicv3_driver_init(void)
{
    return platform_add_driver(&gicv3_driver);
}
INIT_EXPORT_PRE_PIC(gicv3_driver_init, "gicv3 driver init");
