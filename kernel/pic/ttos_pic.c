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
#include <errno.h>
#include <system/types.h>
#include <ttos.h>
#include <ttos_init.h>
#include <ttos_pic.h>
#include <system/kconfig.h>
#include <system/compiler.h>

#define KLOG_LEVEL KLOG_WARN
#define KLOG_TAG "ttos_pic"
#include <klog.h>

/************************宏 定 义******************************/

/************************类型定义******************************/
/************************外部声明******************************/

/************************前向声明******************************/

/************************模块变量******************************/
static struct ttos_pic *ttos_pic_list = NULL;
static struct ttos_pic *cpu_pci = NULL;
static uint32_t virt_irq_index = CONFIG_IRQ_NUMBER_START;
/************************全局变量******************************/

/************************函数实现******************************/
__weak struct ttos_pic *ttos_pic_get_cpu_pic(uint32_t hw_irq)
{
    (void)hw_irq;
    return cpu_pci;
}

int32_t ttos_pic_irq_map(uint32_t virt_irq, struct ttos_pic **pic)
{
    int32_t hw_irq = -1;
    struct ttos_pic *pic_node = ttos_pic_list;

    while (pic_node)
    {
        if (virt_irq >= pic_node->virt_irq_rang[0] && virt_irq < pic_node->virt_irq_rang[1])
        {
            hw_irq = virt_irq - pic_node->virt_irq_rang[0] + pic_node->hwirq_base;
            *pic = pic_node;
            break;
        }

        pic_node = pic_node->next_node;
    }

    // KLOG_I("virt_irq[%d] -> hw_irq[%d]", virt_irq, hw_irq);

    return hw_irq;
}

/**
 * @brief
 *    中断控制器安装
 * @param[in] 无
 * @retval 无
 */
int32_t ttos_pic_register(struct ttos_pic *pic)
{
    struct ttos_pic *pic_node;

    if (ttos_pic_list == NULL)
    {
        ttos_pic_list = pic;
    }
    else
    {
        pic_node = ttos_pic_list;
        while (pic_node->next_node)
        {
            pic_node = pic_node->next_node;
        }

        pic_node->next_node = pic;
    }

    if (!cpu_pci && (pic->flag & PIC_FLAG_CPU))
    {
        cpu_pci = pic;
    }

    return 0;
}

/**
 * @brief
 *    中断控制器卸载
 * @param[in] 无
 * @retval 无
 */
int32_t ttos_pic_unregister(struct ttos_pic *pic)
{
    struct ttos_pic *pic_node;

    if (pic == NULL)
    {
        return -EINVAL;
    }

    if (ttos_pic_list == NULL)
    {
        return 0;
    }

    if (ttos_pic_list == pic)
    {
        ttos_pic_list = ttos_pic_list->next_node;
    }
    else
    {
        pic_node = ttos_pic_list;
        while (pic_node->next_node && pic_node->next_node != pic)
        {
            pic_node = pic_node->next_node;
        }

        if (pic_node->next_node == NULL)
        {
            return -EINVAL;
        }

        pic_node->next_node = pic_node->next_node->next_node;
    }

    return 0;
}

/**
 * @brief
 *    中断控制器初始化
 * @param[in] pic == NULL:初始化链表上的所有中断控制器
 * @param[in] pic != NULL:初始化指定的中断控制器
 * @retval 无
 */
int32_t ttos_pic_init(struct ttos_pic *pic)
{
    int32_t ret;
    uint32_t max_number;
    struct ttos_pic *pic_node;

    KLOG_I("ttos_pic_init");
    if (ttos_pic_list == NULL)
    {
        return -EIO;
    }

    pic_node = ttos_pic_list;

    while (pic_node)
    {
        if ((pic == NULL) || (pic == pic_node))
        {
            ret = -1;
            if (pic_node->pic_ops && pic_node->pic_ops->pic_init &&
                pic_node->pic_ops->pic_max_number_get)
            {
                ret = pic_node->pic_ops->pic_init(pic_node);
            }

            if ((pic_node->virt_irq_rang[0] == pic_node->virt_irq_rang[1]) &&
                (pic_node->virt_irq_rang[0] == 0) && (ret == 0))
            {
                /* 建立中断号映射关系 */
                pic_node->pic_ops->pic_max_number_get(pic_node, &max_number);
                pic_node->virt_irq_rang[0] = pic_node->virt_irq_rang[1] = virt_irq_index;
                pic_node->virt_irq_rang[1] += max_number;
                virt_irq_index = pic_node->virt_irq_rang[1] + 1;
                virt_irq_index = ALIGN_UP(virt_irq_index, 1024);
                KLOG_D("%s virq: [%d-%d]", pic_node->name, pic_node->virt_irq_rang[0],
                       pic_node->virt_irq_rang[1]);
            }
        }

        if (pic == pic_node)
        {
            return 0;
        }

        pic_node = pic_node->next_node;
    }

    return 0;
}

int32_t ttos_pic_irq_affinity_set(uint32_t irq, uint32_t cpu)
{
    struct ttos_pic *pic_node;
    irq = ttos_pic_irq_map(irq, &pic_node);

    if (((int32_t)irq >= 0) && pic_node)
    {
        return pic_node->pic_ops->pic_affinity_set(pic_node, irq, cpu);
    }
    return -EIO;
}

int32_t ttos_pic_pending_set(uint32_t irq)
{
    struct ttos_pic *pic_node;
    irq = ttos_pic_irq_map(irq, &pic_node);

    if (((int32_t)irq >= 0) && pic_node)
    {
        return pic_node->pic_ops->pic_pending_set(pic_node, irq);
    }
    return -EIO;
}

int32_t ttos_pic_pending_get(uint32_t irq, bool *pending)
{
    struct ttos_pic *pic_node;
    irq = ttos_pic_irq_map(irq, &pic_node);

    if (((int32_t)irq >= 0) && pic_node)
    {
        return pic_node->pic_ops->pic_pending_get(pic_node, irq, pending);
    }
    return -EIO;
}

int32_t ttos_pic_irq_priority_set(uint32_t irq, uint32_t priority)
{
    struct ttos_pic *pic_node;
    irq = ttos_pic_irq_map(irq, &pic_node);

    if (((int32_t)irq >= 0) && pic_node)
    {
        return pic_node->pic_ops->pic_priority_set(pic_node, irq, priority);
    }
    return -EIO;
}

int32_t ttos_pic_irq_priority_get(uint32_t irq, uint32_t *priority)
{
    struct ttos_pic *pic_node;
    irq = ttos_pic_irq_map(irq, &pic_node);

    if (((int32_t)irq >= 0) && pic_node)
    {
        return pic_node->pic_ops->pic_priority_get(pic_node, irq, priority);
    }
    return -EIO;
}

int32_t ttos_pic_irq_trigger_mode_set(uint32_t irq, ttos_pic_irq_type_t type)
{
    struct ttos_pic *pic_node;
    irq = ttos_pic_irq_map(irq, &pic_node);

    if (((int32_t)irq >= 0) && pic_node)
    {
        return pic_node->pic_ops->pic_trigger_type_set(pic_node, irq, type);
    }
    return -EIO;
}

/**
 * @brief
 *    ipi中断发生
 * @param[in] irq: CPU硬件中断号
 * @param[in] cpu: 要发送的目的CPU号
 * @retval 无
 */
int32_t ttos_pic_ipi_send(uint32_t hw_irq, uint32_t cpu, uint32_t mode)
{
    struct ttos_pic *pic_node;

    pic_node = ttos_pic_get_cpu_pic(hw_irq);

    if (((int32_t)hw_irq >= 0) && pic_node)
    {
        return pic_node->pic_ops->pic_ipi_send(pic_node, hw_irq, cpu, mode);
    }
    return -EIO;
}

int32_t ttos_pic_irq_mask(uint32_t irq)
{
    struct ttos_pic *pic_node;
    irq = ttos_pic_irq_map(irq, &pic_node);

    if (((int32_t)irq >= 0) && pic_node)
    {
        return pic_node->pic_ops->pic_mask(pic_node, irq);
    }
    return -EIO;
}

int32_t ttos_pic_irq_unmask(uint32_t irq)
{
    struct ttos_pic *pic_node;
    irq = ttos_pic_irq_map(irq, &pic_node);

    if (((int32_t)irq >= 0) && pic_node)
    {
        return pic_node->pic_ops->pic_unmask(pic_node, irq);
    }
    return -EIO;
}

int32_t ttos_pic_irq_ack(uint32_t *src_cpu, uint32_t *irq)
{
    struct ttos_pic *pic_node = ttos_pic_list;
    int32_t ret = -1;

    pic_node = ttos_pic_get_cpu_pic(*irq);

    if (pic_node)
    {
        ret = pic_node->pic_ops->pic_ack(pic_node, src_cpu, irq);
    }

    return ret;
}

int32_t ttos_pic_irq_eoi(uint32_t irq, uint32_t src_cpu)
{
    struct ttos_pic *pic_node = ttos_pic_list;
    irq = ttos_pic_irq_map(irq, &pic_node);

    if (((int32_t)irq >= 0) && pic_node)
    {
        return pic_node->pic_ops->pic_eoi(pic_node, irq, src_cpu);
    }
    return -EIO;
}

int32_t ttos_pic_irq_parse(struct ttos_pic *pic, uint32_t *value_table)
{
    if (pic == NULL)
    {
        return -EINVAL;
    }

    if (pic->pic_ops && pic->pic_ops->pic_dtb_irq_parse)
    {
        return pic->pic_ops->pic_dtb_irq_parse(pic, value_table);
    }

    return -EIO;
}

int32_t ttos_pic_msi_alloc(struct ttos_pic *pic, uint32_t device_id, uint32_t count)
{
    if (pic == NULL)
    {
        return -EINVAL;
    }

    if (pic->pic_ops && pic->pic_ops->pic_msi_alloc)
    {
        return pic->pic_ops->pic_msi_alloc(pic, device_id, count);
    }

    return -EIO;
}

uint32_t ttos_pic_msi_map(struct ttos_pic *pic, uint32_t device_id, uint32_t event_id)
{
    int32_t irq;
    ttos_irq_desc_t desc;

    if (pic == NULL)
    {
        return 0;
    }

    if (pic->pic_ops && pic->pic_ops->pic_msi_map)
    {
        irq = pic->pic_ops->pic_msi_map(pic, device_id, event_id);
        if (irq >= 0)
        {
            desc = ttos_pic_msi_desc_init(irq);
            if (desc)
            {
                desc->pic = pic;
            }
        }
        return irq;
    }

    return 0;
}

uint32_t ttos_pic_msi_send(struct ttos_pic *pic, uint32_t irq)
{
    if (pic == NULL)
    {
        return 0;
    }

    if (pic->pic_ops && pic->pic_ops->pic_msi_send)
    {
        pic->pic_ops->pic_msi_send(pic, irq);
        return 0;
    }
    return -1;
}

uint64_t ttos_pic_msi_address(struct ttos_pic *pic)
{
    if (pic == NULL)
    {
        return 0;
    }

    if (pic->pic_ops && pic->pic_ops->pic_msi_address)
    {
        return pic->pic_ops->pic_msi_address(pic);
    }
    return 0;
}

uint64_t ttos_pic_msi_data(struct ttos_pic *pic, uint32_t index, ttos_irq_desc_t desc)
{
    if (pic == NULL)
    {
        return 0;
    }

    if (pic->pic_ops && pic->pic_ops->pic_msi_data)
    {
        return pic->pic_ops->pic_msi_data(pic, index, desc);
    }
    return 0;
}

void irqdesc_foreach(void (*func) (ttos_irq_desc_t, void *), void *par)
{
    struct ttos_pic *pic_node = ttos_pic_list;
    ttos_irq_desc_t desc;
    int i;

    while (pic_node)
    {
        for (i = pic_node->virt_irq_rang[0];
            i < pic_node->virt_irq_rang[1]; i++)
        {
            desc = ttos_pic_irq_desc_get(i);
            if (desc->used)
            {
                func(desc, par);
            }
        }
        pic_node = pic_node->next_node;
    }

}

int ttos_pic_ipitest_hwirq_get(void)
{
    if(IS_ENABLED(CONFIG_IPI_TEST_HWIRQ))
        return CONFIG_IPI_TEST_HWIRQ;
    return -1;
}

uint64_t ttos_pic_irq_count_get(uint32_t cpuid)
{
    uint64_t total_count = 0;

    if (cpuid >= CONFIG_MAX_CPUS)
    {
        return 0;
    }

    struct ttos_pic *pic_node = ttos_pic_list;

    while (pic_node)
    {
        if (pic_node->flag & PIC_FLAG_CPU)
        {
            total_count += pic_node->count[cpuid];
        }

        pic_node = pic_node->next_node;
    }

    return total_count;
}