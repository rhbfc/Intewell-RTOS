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
#include <driver/devbus.h>
#include <driver/device.h>
#include <errno.h>
#include <shell.h>
#include <spinlock.h>
#include <stdio.h>
#include <stdlib.h>
#include <system/types.h>
#include <ttos_pic.h>

/************************宏 定 义******************************/

/************************类型定义******************************/

/************************外部声明******************************/

/************************前向声明******************************/

/************************模块变量******************************/
/************************全局变量******************************/

/************************函数实现******************************/
static ttos_irq_desc_t  _pic_irq_desc_get(struct ttos_pic *pic, int32_t hwirq)
{
    if (pic->pic_ops && pic->pic_ops->pic_irq_desc_get)
    {
        return pic->pic_ops->pic_irq_desc_get(pic, hwirq);
    }

    return NULL;
}

ttos_irq_desc_t ttos_pic_irq_desc_alloc(struct ttos_pic *pic, uint32_t hw_irq)
{
    ttos_irq_desc_t desc = NULL;

    desc = _pic_irq_desc_get(pic, hw_irq);

    if (!desc->used)
    {
        desc->pic = pic;
        desc->hw_irq = hw_irq;
        desc->virt_irq = pic->virt_irq_rang[0] + hw_irq - pic->hwirq_base;
        desc->used = true;
    }

    return desc;
}

/**
 * @brief
 *    分配CPU中断描述符
 * @param[in] hw_irq CPU的硬件中断号
 * @retval 非NULL 分配成功
 * @retval NULL 分配失败
 */
ttos_irq_desc_t ttos_pic_cpu_irq_desc_alloc(uint32_t hw_irq)
{
    struct ttos_pic *pic = ttos_pic_get_cpu_pic(hw_irq);
    return ttos_pic_irq_desc_alloc(pic, hw_irq);
}

/**
 * @brief
 *    分配中断描述符
 * @param[in] dev dev指针
 * @retval 非NULL 分配成功
 * @retval NULL 分配失败
 */
ttos_irq_desc_t ttos_pic_dev_irq_desc_alloc(struct device *dev)
{
    struct ttos_pic *pic;
    ttos_irq_desc_t desc = NULL;
    int32_t ret, hw_irq;

    ret = of_device_get_resource_pic_irq(dev, &pic, &hw_irq, 1, NULL);
    if ((ret == 0) && (hw_irq >= 0))
    {
        desc = ttos_pic_irq_desc_alloc(pic, (uint32_t)hw_irq);
    }

    return desc;
}

ttos_irq_desc_t ttos_pic_irq_desc_get(uint32_t virt_irq)
{
    struct ttos_pic *pic;
    int32_t hw_irq;

    hw_irq = ttos_pic_irq_map(virt_irq, &pic);
    return _pic_irq_desc_get(pic, hw_irq);
}

/**
 * @brief
 *    释放中断描述符
 * @param[in] irq 中断号
 * @retval 0 释放成功
 * @retval -1 释放失败
 */
int32_t ttos_pic_irq_desc_free(uint32_t virt_irq)
{
    ttos_irq_desc_t desc = NULL;

    desc = ttos_pic_irq_desc_get(virt_irq);
    memset(desc, 0, sizeof(struct ttos_irq_desc));

    return 0;
}

static void show_irq_share_node(ttos_irq_desc_t desc)
{
    ttos_irq_node_t node;
    char str[128];
    int i = 0;

    list_for_each_entry(node, &desc->irq_shared_list, node)
    {
        snprintf(str, 128, ".[%d] %*.*s\n", i, 64, ' ', node->name);
        printk("%s", str);
        i++;
    }
}

static void show_irq_item(ttos_irq_desc_t desc)
{
    char str[128];
    if (desc == NULL)
    {
        snprintf(str, 128, "%-*.*s %-*.*s %-*.*s %-*.*s %-*.*s %-*.*s %-*.*s\r\n", 10, ' ', "vIRQ",
                 10, ' ', "hwIRQ", 10, ' ', "bind-map", 10, ' ', "count", 10, ' ', "shared", 16,
                 ' ', "name", 10, ' ', "PIC");
    }
    else
    {
        snprintf(str, 128, "%-10d %-10d %-10llX %-10d %c          %-*.*s %-*.*s\r\n",
                 desc->virt_irq, desc->hw_irq, desc->cpumap, desc->count,
                 (desc->flags & IRQ_SHARED) ? 'Y' : 'N', 16, ' ', desc->name, 10, ' ',
                 desc->pic->name);
    }

    printk("%s", str);

    if (desc && desc->flags & IRQ_SHARED)
        show_irq_share_node(desc);
}

/**
 * @brief
 *    show中断信息
 * @param[in] 无
 * @retval 无
 */
void irq_show(void)
{
    struct ttos_pic *pic = ttos_pic_get_cpu_pic(0);
    ttos_irq_desc_t desc;
    uint32_t i;

    show_irq_item(NULL);

    for (i = pic->virt_irq_rang[0]; i < pic->virt_irq_rang[1]; i++)
    {
        desc = ttos_pic_irq_desc_get(i);
        if (desc && desc->used)
        {
            show_irq_item(desc);
        }
    }
}

#include <shell.h>
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) |
                     SHELL_CMD_DISABLE_RETURN,
                 irq, irq_show, irq show);
