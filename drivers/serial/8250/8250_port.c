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
 * @file    drivers/serial/8250/uart_i8250.c
 * @author  TTOS Driver Team (重构 by uart_core)
 * @brief   Intel 8250 UART 驱动
 * @version 2.0.0
 * @date    2025-10-10
 *
 * 重构说明：
 * - 从 173 行减少到 ~140 行（20% 缩减）
 * - 使用 uart_core 统一框架
 * - 使用 8250_port 通用操作
 */

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include <driver/device.h>
#include <driver/driver.h>
#include <driver/serial/serial_8250.h>
#include <driver/serial/serial_core.h>
#include <driver/serial/serial_reg.h>
#include <io.h>
#include <system/kconfig.h>
#include <ttosMM.h>
#include <ttos_init.h>
#include <ttos_pic.h>

#undef KLOG_TAG
#define KLOG_TAG "i8250"
#include <klog.h>

/****************************************************************************
 * i8250 配置
 ****************************************************************************/

static unsigned long uart_com[] = {0x3f8, 0x2f8, 0x3e8, 0x2e8};
static unsigned long uart_irq[] = {4, 3, 4, 3};

/****************************************************************************
 * i8250 特定的初始化
 ****************************************************************************/

/**
 * i8250 硬件初始化
 */
static void i8250_hw_init(struct uart_port *port)
{
    serialout(port, 8, 1);
    serialout(port, 8, 0);

    serialout(port, UART_IER_OFFSET, 0);

    serialout(port, 4, 0);
    serialout(port, 4, serialin(port, 4) | 0x02);

    serialout(port, 3, serialin(port, 3) | 0x80);
    serialout(port, 3, serialin(port, 3) & ~0x80);
    serialout(port, 3, (serialin(port, 3) & ~0x80) | 0x3);
    serialout(port, 3, (serialin(port, 3) & ~0x4));
    serialout(port, 3, (serialin(port, 3) & ~0x8));

    serialout(port, UART_IER_OFFSET, 0x2);
}

/**
 * 配置端口地址和中断号
 */
static int i8250_setup_port(struct uart_port *port, int com_port)
{
    port->io_base = uart_com[com_port];
    port->hw_irq = uart_irq[com_port];

    return 0;
}

static int i8250_register(int com_port)
{
    struct device *dev;
    struct uart_8250_port *i8250;
    int ret;

    /* 手动创建 device（x86 平台） */
    dev = (struct device *)calloc(1, sizeof(struct device));
    if (!dev)
    {
        KLOG_E("Failed to allocate device");
        return -ENOMEM;
    }

    initialize_device(dev);
    strcpy(dev->name, "i8250_uart");

    /* 使用托管内存分配（绑定到 dev） */
    i8250 = devm_kzalloc(dev, sizeof(*i8250), GFP_KERNEL);
    if (!i8250)
    {
        free(dev);
        return -ENOMEM;
    }

    /* 初始化 uart_port */
    i8250->port.dev = dev;
    i8250->port.ops = &uart_8250_ops; /* 使用 8250 通用 ops */
    i8250->port.io_type = UART_IO_PORT;
    i8250->port.config.baudrate = CONFIG_UART_BAUD;
    i8250->port.config.data_bits = 8;
    i8250->port.config.stop_bit = 0;
    i8250->port.config.parity = 0;
    i8250->port.config.uartclk = 1843200; /* Standard PC UART clock */
    i8250->port.is_console = true;
    i8250->use_fifo = false; /* i8250 不使用 FIFO */

    dev->priv = &i8250->port;

    /* 配置端口地址和中断号 */
    ret = i8250_setup_port(&i8250->port, com_port);
    if (ret < 0)
    {
        free(dev);
        return ret;
    }

    /* 设置 IO 访问函数 */
    uart_set_io_from_type(&i8250->port);

    /* i8250 特定的硬件初始化 */
    i8250_hw_init(&i8250->port);

    uart8250_init_config(&i8250->port);

    /* 分配中断号 */
    i8250->port.irq = ttos_pic_irq_alloc(NULL, i8250->port.hw_irq);

    /* 使用托管中断注册 */
    ret = devm_request_irq(dev, i8250->port.irq, uart_8250_interrupt, 0, "i8250", dev);
    if (ret)
    {
        KLOG_E("Failed to request IRQ %d, ret=%d", i8250->port.irq, ret);
        free(dev);
        return ret;
    }

    /* 初始化时先 mask 中断 */
    ttos_pic_irq_mask(i8250->port.irq);

    /* 使用统一的 UART 注册函数 */
    ret = uart_register_port(dev);
    if (ret < 0)
    {
        KLOG_E("Failed to register UART port");
        free(dev);
        return ret;
    }

    KLOG_I("i8250 UART initialized successfully");
    return 0;
}

static int probe_bios_serial(void)
{
#if defined(CONFIG_FORCE_COM_NR) && (CONFIG_FORCE_COM_NR > 0)
    for (int i = 0; i < CONFIG_FORCE_COM_NR; i++)
    {
        i8250_register(i);
        KLOG_I("8250ISA Force Probe 0x%X", uart_com[i]);
    }
#else
    const unsigned short *bios = (const unsigned short *)ioremap(0x400, 0x1000);
    for (int i = 0; i < 4; i++)
    {
        if (readw(&bios[i]) != 0)
        {
            i8250_register(i);
            KLOG_I("8250ISA Probe 0x%X", readw(&bios[i]));
        }
    }
    iounmap((virt_addr_t)bios);
#endif
    return 0;
}

#if defined(__x86_64__) && !defined(CONFIG_ACPI)
INIT_EXPORT_DRIVER(probe_bios_serial, "i8250 driver init");
#endif
