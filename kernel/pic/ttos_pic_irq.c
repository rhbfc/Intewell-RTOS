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
#include <driver/device.h>
#include <errno.h>
#include <shell.h>
#include <spinlock.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ttos_pic.h>
#include <system/types.h>
#include <trace.h>
#include <time/ktime.h>
#include <cpuid.h>

#define KLOG_TAG "ttos_pic"
#include <klog.h>

/************************宏 定 义******************************/

/************************类型定义******************************/

/************************外部声明******************************/

/************************前向声明******************************/

/************************模块变量******************************/
static DEFINE_SPINLOCK (irq_lock);

/************************全局变量******************************/

/************************函数实现******************************/

extern void printk(const char *fmt, ...);

/**
 * @brief
 *    默认中断处理函数
 * @param[in] irq 产生的中断号
 * @param[in] arg 参数
 * @retval 无
 */
static void ttos_pic_irq_default_handler (uint32_t irq, void *arg)
{
    printk ("irq %d into irq_default_handler\n", irq);
}

/**
 * @brief
 *    安装非共享中断处理程序
 * @param[in] irq 中断号
 * @param[in] handler 中断处理程序
 * @param[in] arg 中断处理程序入参
 * @param[in] flags 中断标志IRQ_SHARED-共享中断 IRQ_ALLOC-分配中断
 * @param[in] name 中断名字
 * @retval 0  安装成功
 * @retval -EINVAL 参数无效
 */
static int32_t ttos_pic_irq_install_unshared (ttos_irq_desc_t irq_desc,
                                              isr_handler_t handler, void *arg,
                                              uint32_t flags, const char *name)
{
    int32_t len;

    if (!irq_desc)
    {
        return -EINVAL;
    }

    len = strlen ((const char *)name) + 1;
    if (len > CONFIG_IRQ_NAME_MAX)
    {
        len = CONFIG_IRQ_NAME_MAX;
    }

    memcpy (irq_desc->name, name, len);
    irq_desc->arg = arg;
    irq_desc->flags |= flags;

    if (handler)
    {
        irq_desc->handler = handler;
    }

    return 0;
}

/**
 * @brief
 *    安装共享中断处理程序
 * @param[in] irq 中断号
 * @param[in] handler 中断处理程序
 * @param[in] arg 中断处理程序入参
 * @param[in] flags 中断标志IRQ_SHARED-共享中断 IRQ_ALLOC-分配中断
 * @param[in] name 中断名字
 * @retval >0  返回分配的irq号
 * @retval 0   安装成功
 * @retval -EINVAL 参数无效
 * @retval -ENOMEM 空间不足
 * @retval -EBUSY  已经存在的中断不允许共享
 */
static int32_t ttos_pic_irq_install_shared (ttos_irq_desc_t irq_desc,
                                            isr_handler_t handler, void *arg,
                                            int32_t flags, const char *name)
{
    size_t          len;
    size_t          irq_flags;
    ttos_irq_node_t irq_node;

    if (!irq_desc || !irq_desc->used)
    {
        return -EINVAL;
    }

    spin_lock_irqsave (&irq_lock, irq_flags);

    irq_node = (ttos_irq_node_t)malloc (sizeof (struct ttos_irq_node));
    if (!irq_node)
    {
        return -ENOMEM;
    }

    memset(irq_node, 0, sizeof(struct ttos_irq_node));

    len = strlen ((const char *)name) + 1;
    if (len > CONFIG_IRQ_NAME_MAX)
    {
        len = CONFIG_IRQ_NAME_MAX;
    }

    memcpy(irq_node->name, name, len);
    if (!irq_desc->name[0])
    {
        memcpy(irq_desc->name, name, len);
    }
    irq_node->handler = handler;
    irq_node->arg     = arg;
    irq_desc->flags  |= flags;

    if (!irq_desc->irq_shared_list.prev)
    {
        INIT_LIST_HEAD(&irq_desc->irq_shared_list);
    }

    INIT_LIST_HEAD(&irq_node->node);
    list_add_tail (&irq_node->node, &irq_desc->irq_shared_list);
    spin_unlock_irqrestore (&irq_lock, irq_flags);

    return 0;
}

/**
 * @brief
 *    安装中断处理程序
 * @param[in] irq
 * irq号：若flags有IRQ_ALLOC标记,该参数无效，表示先分配一个irq，再安装handler
 * @param[in] handler 中断处理程序
 * @param[in] arg 中断处理程序入参
 * @param[in] flags 中断标志 IRQ_SHARED-共享中断 IRQ_ALLOC-分配中断
 * @param[in] name 中断名字
 * @retval 0 安装成功
 * @retval > 0  成功分配的irq号，并安装handler成功
 * @retval -EINVAL   参数无效
 */
int32_t ttos_pic_irq_install (uint32_t irq, isr_handler_t handler, void *arg,
                              uint32_t flags, const char *name)
{
    ttos_irq_desc_t irq_desc = NULL;
    int ret;

    if (!handler)
    {
        return -EINVAL;
    }

    if (!name)
    {
        name = (const char *)"none";
    }

    irq_desc = ttos_pic_irq_desc_get (irq);
    if (!irq_desc)
    {
        KLOG_E ("virq[%d] {%s} install faild, irq_desc NULL.", irq, name);
        return -EINVAL;
    }

    if (!irq_desc->used)
    {
        KLOG_E ("virq[%d]  hwirq[%d] {%s} install faild, irq_desc unsed.", irq,
                irq_desc->hw_irq, name);
        return -EINVAL;
    }

    if (IRQ_SHARED & flags)
    {
        /* 安装共享中断 */
        ret = ttos_pic_irq_install_shared (irq_desc, handler, arg, flags,
                                            name);
    }
    else
    {
        /* 安装非共享中断 */
        ret = ttos_pic_irq_install_unshared (irq_desc, handler, arg, flags,
                                              name);
    }

    trace_object(TRACE_OBJ_IRQ, irq_desc);

    return ret;
}

/**
 * @brief
 *    卸载中断处理程序
 * @param[in] irq 中断号
 * @param[in] name 针对共享中断,该值才有意义,用作区分不同的handler
 * @retval 0  卸载成功
 * @retval -EINVAL 参数无效
 */
int32_t ttos_pic_irq_uninstall (uint32_t irq, const char *name)
{
    int32_t         ret = 0;
    size_t          irq_flags;
    ttos_irq_node_t irq_tmp_node;
    ttos_irq_node_t irq_node;
    ttos_irq_desc_t irq_desc = NULL;

    irq_desc = ttos_pic_irq_desc_get (irq);

    /* 检查irq对应的irq_desc是否已经存在和已被使用 */
    if (!irq_desc || !irq_desc->used)
    {
        return -EINVAL;
    }

    spin_lock_irqsave (&irq_lock, irq_flags);

    /* 为共享中断，则需要释放节点空间 */
    if (IRQ_SHARED & irq_desc->flags)
    {
        /* 共享中断需要依赖名字 */
        if (!name)
        {
            spin_unlock_irqrestore (&irq_lock, irq_flags);
            return -EINVAL;
        }

        /* 遍历链表*/
        list_for_each_entry_safe (irq_node, irq_tmp_node,
                                  &irq_desc->irq_shared_list, node)
        {
            if (!strcmp ((const char *)irq_node->name, (const char *)name))
            {
                /* 删除节点，释放节点 */
                list_delete (&irq_node->node);
                free (irq_node);
                break;
            }
        }

        /* 链表为空，释放描述符 */
        if (list_is_empty (&irq_desc->irq_shared_list))
        {
            ret = ttos_pic_irq_desc_free (irq);
        }
    }
    else
    {
        ret = ttos_pic_irq_desc_free (irq);
    }

    spin_unlock_irqrestore (&irq_lock, irq_flags);
    return ret;
}

/**
 * @brief
 *    调用中断处理程序
 * @param[in] irq 中断号
 * @retval 无
 */
void ttos_pic_irq_handle (uint32_t irq)
{
    ttos_irq_desc_t desc;
    ttos_irq_node_t irq_node;
    uint64_t timestamp;

    timestamp = ktime_get_ns();

    desc = ttos_pic_irq_desc_get(irq);

    if (desc)
    {
        desc->count++;
        desc->pic->count[cpuid_get()]++;

        /* 非共享中断，直接调用handler */
        if (!(IRQ_SHARED & desc->flags))
        {
            if (desc->handler)
            {
                desc->handler(irq, desc->arg);
            }
            else
            {
                KLOG_W("irq (%d) handler is NULL", desc->hw_irq);
            }
        }
        else
        {
            /* 共享中断,遍历链表，调用handler */
            list_for_each_entry (irq_node, &desc->irq_shared_list, node)
            {
                if (irq_node->handler)
                {
                    irq_node->handler(desc->virt_irq, irq_node->arg);
                }
                else
                {
                    KLOG_W("irq (%d) handler is NULL", desc->hw_irq);
                }
            }
        }
    }
    else
    {
                KLOG_E("irq v:%d irq desc not exist!", irq);
    }

    trace_irq(irq, timestamp, ktime_get_ns() - timestamp);
}

/**
 * @brief
 *    中断irq分配
 * @param[in] dev 设备句柄
 * @param[in] cpu_hw_irq cpu硬件中断号
 * @retval 0:分配失败
 * @retval >0:虚拟中断号
 */
uint32_t ttos_pic_irq_alloc (struct device *dev, uint32_t cpu_hw_irq)
{
    ttos_irq_desc_t desc;

    if (dev)
    {
        desc = ttos_pic_dev_irq_desc_alloc (dev);
    }
    else
    {
        desc = ttos_pic_cpu_irq_desc_alloc (cpu_hw_irq);
    }

    if (desc)
    {
        if (!desc->handler)
        {
            desc->handler = ttos_pic_irq_default_handler;
        }

        desc->used = true;
        return desc->virt_irq;
    }

    return 0;
}

ttos_irq_desc_t ttos_pic_msi_desc_init(uint32_t irq)
{
    ttos_irq_desc_t desc = NULL;

    desc = ttos_pic_irq_desc_get(irq);

    if (desc)
    {
        if (!desc->handler)
        {
            desc->handler = ttos_pic_irq_default_handler;
        }

        desc->virt_irq = irq;
        desc->used = true;
    }

    return desc;
}