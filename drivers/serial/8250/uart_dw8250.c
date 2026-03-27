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

/**
 * @file    drivers/serial/8250/uart_dw8250.c
 * @author  TTOS Driver Team (重构 by uart_core)
 * @brief   DesignWare 8250 UART 驱动
 * @version 2.0.0
 * @date    2025-10-10
 *
 * 重构说明：
 * - 从 710 行减少到 ~200 行（72% 缩减）
 * - 使用 uart_core 统一框架
 * - 使用 8250_port 通用操作
 * - 只保留 DW8250 特定的初始化代码
 */

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

// #include <driver/clk.h>
#include <driver/devbus.h>
#include <driver/device.h>
#include <driver/driver.h>
#include <driver/of.h>
#include <driver/platform.h>
#include <driver/serial/serial_8250.h>
#include <driver/serial/serial_core.h>
#include <driver/serial/serial_reg.h>
#include <io.h>
#include <ttosMM.h>
#include <ttos_init.h>
#include <ttos_pic.h>

#undef KLOG_TAG
#define KLOG_TAG "DW8250"
#include <klog.h>

/****************************************************************************
 * DW8250 特定的初始化
 ****************************************************************************/

 static uint32_t dw_port_readl(struct uart_port *up, uint32_t offset)
{
    return readl((uint32_t *)up->membase + offset);
}

static void dw_port_writel(struct uart_port *up, uint32_t offset, uint32_t value)
{
    if (offset == UART_LCR_OFFSET)
    {
        /* Writeable only when UART is not busy (USR[0] is zero) */
        while (dw_port_readl(up, UART_USR_OFFSET) & UART_USR_BUSY);
    }
    writel(value, (uint32_t *)up->membase + offset);
}

/**
 * DW8250 硬件初始化
 * 这是 DW8250 特有的寄存器配置
 */
static void dw8250_hw_init(struct uart_port *port)
{
    /* DW8250 特有的初始化序列 */
    serialout(port, 8, 1);
    serialout(port, 8, 0);

    /* 禁用所有中断 */
    serialout(port, UART_IER_OFFSET, 0);

    /* FIFO 初始化 */
    serialout(port, UART_FCR_OFFSET, 0x17);

    /* MCR 设置 */
    serialout(port, 4, 0);
    serialout(port, 4, serialin(port, 4) | 0x02);

    /* LCR 设置 */
    serialout(port, 3, serialin(port, 3) | 0x80);
    serialout(port, 3, serialin(port, 3) & ~0x80);
    serialout(port, 3, (serialin(port, 3) & ~0x80) | 0x3);
    serialout(port, 3, (serialin(port, 3) & ~0x4));
    serialout(port, 3, (serialin(port, 3) & ~0x8));

    /* 清除状态 */
    serialin(port, UART_LSR_OFFSET);
    serialin(port, UART_IIR_OFFSET);
    serialin(port, UART_USR_OFFSET);
}

/****************************************************************************
 * 驱动接口
 ****************************************************************************/

static int dw8250_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct uart_8250_port *dw8250;
    phys_addr_t reg_addr;
    size_t reg_size;
    int ret;

    /* 使用托管内存分配 */
    dw8250 = devm_kzalloc(dev, sizeof(*dw8250), GFP_KERNEL);
    if (!dw8250)
        return -ENOMEM;

    /* 初始化 uart_port */
    dw8250->port.dev = dev;
    dw8250->port.ops = &uart_8250_ops; /* 使用 8250 通用 ops */
    dw8250->port.io_type = UART_IO_MEM32;
    dw8250->port.config.baudrate = CONFIG_UART_BAUD;
    dw8250->port.config.data_bits = 8;
    dw8250->port.config.stop_bit = 0;
    dw8250->port.config.parity = 0;
    dw8250->port.config.uartclk = 24000000;
    dw8250->port.is_console = true;
    dw8250->use_fifo = true;

    dev->priv = &dw8250->port;

    /* 获取寄存器地址并映射 */
    ret = of_device_get_resource_reg(dev, &reg_addr, &reg_size);
    if (ret < 0)
    {
        KLOG_E("Failed to get register address");
        return ret;
    }

    KLOG_I("reg: 0x%" PRIx64 ", size: 0x%" PRIxPTR, reg_addr, reg_size);

    /* 使用托管 I/O 映射 */
    dw8250->port.membase = (void *)devm_ioremap(dev, reg_addr, reg_size);
    if (!dw8250->port.membase)
        return -ENOMEM;

    /* 设置 IO 访问函数 */
    dw8250->port.serial_in = dw_port_readl;
    dw8250->port.serial_out = dw_port_writel;

    /* DW8250 特定的硬件初始化 */
    dw8250_hw_init(&dw8250->port);

#ifdef CONFIG_CLK
    /* Clock 配置 */
    ret = clk_get_by_index(dev, 0, &dw8250->mclk);
    if (ret == 0)
    {
        clk_set_rate(&dw8250->mclk, dw8250->port.config.uartclk);
        clk_enable(&dw8250->mclk);
    }
#endif

    /* 分配中断号 */
    dw8250->port.irq = ttos_pic_irq_alloc(dev, 0);

    /* 使用托管中断注册（使用 8250 专用中断处理） */
    ret = devm_request_irq(dev, dw8250->port.irq, uart_8250_interrupt, 0, "dw8250", dev);
    if (ret)
    {
        KLOG_E("Failed to request IRQ %d, ret=%d", dw8250->port.irq, ret);
        return ret;
    }

    uart8250_init_config(&dw8250->port);

    /* 初始化时先 mask 中断 */
    ttos_pic_irq_mask(dw8250->port.irq);

    /* 使用统一的 UART 注册函数 */
    ret = uart_register_port(dev);
    if (ret < 0)
    {
        KLOG_E("Failed to register UART port");
        return ret;
    }

    KLOG_I("DW8250 UART initialized successfully");
    return 0;
}

static void dw8250_remove(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
#ifdef CONFIG_CLK
    struct uart_8250_port *dw8250 = (struct uart_8250_port *)dev->priv;

    /* 禁用 clock */
    if (clk_valid(&dw8250->mclk))
    {
        clk_disable(&dw8250->mclk);
    }
#endif

    /* 所有资源由 devres 自动释放 */
    uart_unregister_port(dev);
}

static struct of_device_id dw8250_table[] = {
    {.compatible = "snps,dw-apb-uart"},
    {/* end of list */},
};

static struct platform_driver dw8250_driver = {
    .probe = dw8250_probe,
    .remove = dw8250_remove,
    .driver =
        {
            .name = "dw8250",
            .of_match_table = &dw8250_table[0],
        },
};

static int __dw8250_init(void)
{
    return platform_add_driver(&dw8250_driver);
}

INIT_EXPORT_DRIVER(__dw8250_init, "dw8250 driver init");
