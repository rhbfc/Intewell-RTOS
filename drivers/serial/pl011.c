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
 * @file    drivers/serial/pl011.c
 * @author  TTOS Driver Team (重构 by uart_core)
 * @brief   ARM PL011 UART 驱动 - 使用统一 UART 框架
 * @version 4.0.0
 * @date    2025-10-10
 *
 * 重构说明：
 * - 从 800 行减少到 ~400 行（50% 缩减）
 * - 使用 uart_core 统一框架
 * - 消除重复的缓冲区管理代码
 * - 使用通用中断处理
 * - 简化错误处理路径
 */

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <driver/devbus.h>
#include <driver/device.h>
#include <driver/driver.h>
#include <driver/of.h>
#include <driver/platform.h>
#include <driver/serial/serial_core.h>
#include <ttosMM.h>
#include <ttos_init.h>
#include <ttos_pic.h>

#undef KLOG_TAG
#define KLOG_TAG "pl011"
#include <klog.h>

#ifdef CONFIG_UART_PL011

/***************************************************************************
 * PL011 寄存器定义
 ***************************************************************************/

#ifndef BIT
#define BIT(n) (1UL << (n))
#endif

/* Flags Register (FR) */
#define PL011_FR_CTS BIT(0)
#define PL011_FR_DSR BIT(1)
#define PL011_FR_DCD BIT(2)
#define PL011_FR_BUSY BIT(3)
#define PL011_FR_RXFE BIT(4) /* RX FIFO empty */
#define PL011_FR_TXFF BIT(5) /* TX FIFO full */
#define PL011_FR_RXFF BIT(6) /* RX FIFO full */
#define PL011_FR_TXFE BIT(7) /* TX FIFO empty */
#define PL011_FR_RI BIT(8)

/* Line Control Register (LCRH) */
#define PL011_LCRH_BRK BIT(0)
#define PL011_LCRH_PEN BIT(1)  /* Parity enable */
#define PL011_LCRH_EPS BIT(2)  /* Even parity */
#define PL011_LCRH_STP2 BIT(3) /* 2 stop bits */
#define PL011_LCRH_FEN BIT(4)  /* FIFO enable */
#define PL011_LCRH_WLEN_SHIFT 5
#define PL011_LCRH_WLEN_WIDTH 2
#define PL011_LCRH_SPS BIT(7)

#define PL011_LCRH_WLEN_SIZE(x) ((x)-5)

/* Control Register (CR) */
#define PL011_CR_UARTEN BIT(0) /* UART enable */
#define PL011_CR_SIREN BIT(1)
#define PL011_CR_SIRLP BIT(2)
#define PL011_CR_LBE BIT(7) /* Loopback enable */
#define PL011_CR_TXE BIT(8) /* TX enable */
#define PL011_CR_RXE BIT(9) /* RX enable */
#define PL011_CR_DTR BIT(10)
#define PL011_CR_RTS BIT(11)
#define PL011_CR_Out1 BIT(12)
#define PL011_CR_Out2 BIT(13)
#define PL011_CR_RTSEn BIT(14)
#define PL011_CR_CTSEn BIT(15)

/* Interrupt Mask Set/Clear Register (IMSC) */
#define PL011_IMSC_RIMIM BIT(0)
#define PL011_IMSC_CTSMIM BIT(1)
#define PL011_IMSC_DCDMIM BIT(2)
#define PL011_IMSC_DSRMIM BIT(3)
#define PL011_IMSC_RXIM BIT(4)  /* RX interrupt */
#define PL011_IMSC_TXIM BIT(5)  /* TX interrupt */
#define PL011_IMSC_RTIM BIT(6)  /* RX timeout */
#define PL011_IMSC_FEIM BIT(7)  /* Framing error */
#define PL011_IMSC_PEIM BIT(8)  /* Parity error */
#define PL011_IMSC_BEIM BIT(9)  /* Break error */
#define PL011_IMSC_OEIM BIT(10) /* Overrun error */

#define PL011_IMSC_MASK_ALL                                                                        \
    (PL011_IMSC_OEIM | PL011_IMSC_BEIM | PL011_IMSC_PEIM | PL011_IMSC_FEIM | PL011_IMSC_RIMIM |    \
     PL011_IMSC_CTSMIM | PL011_IMSC_DCDMIM | PL011_IMSC_DSRMIM | PL011_IMSC_RXIM |                 \
     PL011_IMSC_TXIM | PL011_IMSC_RTIM)

/* Baud Rate Registers */
#define PL011_FBRD_WIDTH 6

/***************************************************************************
 * PL011 寄存器映射
 ***************************************************************************/

struct pl011_regs
{
    uint32_t dr; /* 0x00: Data Register */
    union {
        uint32_t rsr;
        uint32_t ecr;
    }; /* 0x04: Receive Status / Error Clear */
    uint32_t reserved_0[4];
    uint32_t fr; /* 0x18: Flag Register */
    uint32_t reserved_1;
    uint32_t ilpr;  /* 0x20: IrDA Low-Power */
    uint32_t ibrd;  /* 0x24: Integer Baud Rate */
    uint32_t fbrd;  /* 0x28: Fractional Baud Rate */
    uint32_t lcr_h; /* 0x2C: Line Control */
    uint32_t cr;    /* 0x30: Control */
    uint32_t ifls;  /* 0x34: Interrupt FIFO Level Select */
    uint32_t imsc;  /* 0x38: Interrupt Mask Set/Clear */
    uint32_t ris;   /* 0x3C: Raw Interrupt Status */
    uint32_t mis;   /* 0x40: Masked Interrupt Status */
    uint32_t icr;   /* 0x44: Interrupt Clear */
    uint32_t dmacr; /* 0x48: DMA Control */
};

/***************************************************************************
 * PL011 私有数据
 ***************************************************************************/

struct pl011_port
{
    struct uart_port port;            /* 必须放第一个 */
    volatile struct pl011_regs *regs; /* 寄存器基址 */
    bool sbsa;                        /* Server Base System Architecture 模式 */
};

static inline struct pl011_port *to_pl011_port(struct uart_port *port)
{
    return container_of(port, struct pl011_port, port);
}

/***************************************************************************
 * PL011 硬件操作函数（实现 uart_ops）
 ***************************************************************************/

/**
 * 设置波特率
 */
static int pl011_set_baudrate(struct pl011_port *pl011, uint32_t clk, uint32_t baudrate)
{
    volatile struct pl011_regs *regs = pl011->regs;

    /* 计算分频值: bauddiv = (clk * 64) / (baudrate * 16) */
    uint64_t bauddiv = (((uint64_t)clk) << PL011_FBRD_WIDTH) / (baudrate * 16U);

    if ((bauddiv < (1U << PL011_FBRD_WIDTH)) || (bauddiv > (65535U << PL011_FBRD_WIDTH)))
    {
        return -EINVAL;
    }

    regs->ibrd = bauddiv >> PL011_FBRD_WIDTH;
    regs->fbrd = bauddiv & ((1U << PL011_FBRD_WIDTH) - 1U);

    /* 写入 lcr_h 使分频值生效 (ARM DDI 0183F, Pg 3-13) */
    regs->lcr_h = regs->lcr_h;

    return 0;
}

static int pl011_get_baudrate(struct pl011_port *pl011, uint32_t clk)
{
    volatile struct pl011_regs *regs = pl011->regs;
    uint32_t ibrd = regs->ibrd;
    uint32_t fbrd = regs->fbrd;
    uint32_t bauddiv = (ibrd << PL011_FBRD_WIDTH) + fbrd;

    if (bauddiv == 0)
    {
        return CONFIG_UART_BAUD;
    }
    uint32_t baudrate = (clk * 64) / (bauddiv * 16U);

    return baudrate;
}

/**
 * 实现 uart_ops->set_termios
 * 配置波特率、数据位、停止位、校验位
 */
static int pl011_set_termios(struct uart_port *port)
{
    struct pl011_port *pl011 = to_pl011_port(port);
    volatile struct pl011_regs *regs = pl011->regs;
    uint32_t lcrh;
    int ret;

    /* SBSA 模式由固件配置，跳过 */
    if (pl011->sbsa)
        return 0;

    /* 禁用 UART */
    regs->cr &= ~PL011_CR_UARTEN;
    regs->lcr_h &= ~PL011_LCRH_FEN; /* 禁用 FIFO */

    /* 设置波特率 */
    ret = pl011_set_baudrate(pl011, port->config.uartclk, port->config.baudrate);
    if (ret != 0)
        return ret;

    /* 配置数据位、停止位、校验位 */
    lcrh = regs->lcr_h & ~((3 << PL011_LCRH_WLEN_SHIFT) | PL011_LCRH_PEN | PL011_LCRH_EPS |
                           PL011_LCRH_STP2 | PL011_LCRH_BRK);

    /* 数据位 */
    lcrh |= PL011_LCRH_WLEN_SIZE(port->config.data_bits) << PL011_LCRH_WLEN_SHIFT;

    /* 停止位 */
    if (port->config.stop_bit)
        lcrh |= PL011_LCRH_STP2;

    /* 校验位 */
    if (port->config.parity == 1)
        lcrh |= PL011_LCRH_PEN; /* 奇校验 */
    else if (port->config.parity == 2)
        lcrh |= PL011_LCRH_PEN | PL011_LCRH_EPS; /* 偶校验 */

    regs->lcr_h = lcrh;

    /* 清除所有中断 */
    regs->imsc = 0U;
    regs->icr = PL011_IMSC_MASK_ALL;

    /* 配置控制寄存器 */
    regs->dmacr = 0U;
    regs->cr &= ~(BIT(14) | BIT(15) | BIT(1));
    regs->cr |= PL011_CR_RXE | PL011_CR_TXE;

    return 0;
}

static int pl011_init_config(struct uart_port *port)
{
    struct pl011_port *pl011 = to_pl011_port(port);
    volatile struct pl011_regs *regs = pl011->regs;
    uint32_t lcrh = regs->lcr_h;

    if (lcrh & PL011_LCRH_PEN)
    {
        port->config.parity = (lcrh & PL011_LCRH_EPS) ? 2 : 1;
    }

    port->config.data_bits = PL011_LCRH_WLEN_SIZE(lcrh >> PL011_LCRH_WLEN_SHIFT);
    if (port->config.data_bits == 0)
    {
        port->config.data_bits = 8;
    }
    port->config.stop_bit = !!(lcrh & PL011_LCRH_STP2);
    port->config.baudrate = pl011_get_baudrate(pl011, port->config.uartclk);

    return 0;
}

/**
 * 实现 uart_ops->startup
 * 启动 UART
 */
static int pl011_startup(struct uart_port *port)
{
    struct pl011_port *pl011 = to_pl011_port(port);

    if (!pl011->sbsa)
    {
        pl011->regs->cr |= PL011_CR_UARTEN;
    }

    return 0;
}

/**
 * 实现 uart_ops->shutdown
 * 停止 UART
 */
static void pl011_shutdown(struct uart_port *port)
{
    struct pl011_port *pl011 = to_pl011_port(port);

    if (!pl011->sbsa)
    {
        pl011->regs->cr &= ~PL011_CR_UARTEN;
    }
}

/**
 * 实现 uart_ops->tx_char
 * 发送单个字符
 */
static void pl011_tx_char(struct uart_port *port, int ch)
{
    struct pl011_port *pl011 = to_pl011_port(port);
    pl011->regs->dr = ch;
}

/**
 * 实现 uart_ops->rx_char
 * 接收单个字符
 */
static int pl011_rx_char(struct uart_port *port, unsigned int *status)
{
    struct pl011_port *pl011 = to_pl011_port(port);
    uint32_t data = pl011->regs->dr;

    *status = 0; /* TODO: 提取错误状态位 */
    return data & 0xFF;
}

/**
 * 实现 uart_ops->tx_ready
 * 检查是否可以发送
 */
static bool pl011_tx_ready(struct uart_port *port)
{
    struct pl011_port *pl011 = to_pl011_port(port);

    if (!pl011->sbsa && !(pl011->regs->cr & PL011_CR_TXE))
        return false;

    return (pl011->regs->imsc & PL011_IMSC_TXIM) && (pl011->regs->fr & PL011_FR_TXFE);
}

/**
 * 实现 uart_ops->tx_empty
 * 检查发送 FIFO 是否为空
 */
static bool pl011_tx_empty(struct uart_port *port)
{
    struct pl011_port *pl011 = to_pl011_port(port);
    return (pl011->regs->fr & PL011_FR_TXFE) != 0;
}

/**
 * 实现 uart_ops->rx_ready
 * 检查是否有数据可接收
 */
static bool pl011_rx_ready(struct uart_port *port)
{
    struct pl011_port *pl011 = to_pl011_port(port);

    if (!pl011->sbsa && !(pl011->regs->cr & PL011_CR_RXE))
        return false;

    return (pl011->regs->imsc & PL011_IMSC_RXIM) && !(pl011->regs->fr & PL011_FR_RXFE);
}

/**
 * 实现 uart_ops->start_tx
 * 启动发送中断
 */
static void pl011_start_tx(struct uart_port *port)
{
    struct pl011_port *pl011 = to_pl011_port(port);
    pl011->regs->imsc |= PL011_IMSC_TXIM;
}

/**
 * 实现 uart_ops->stop_tx
 * 停止发送中断
 */
static void pl011_stop_tx(struct uart_port *port)
{
    struct pl011_port *pl011 = to_pl011_port(port);
    pl011->regs->imsc &= ~PL011_IMSC_TXIM;
}

/**
 * 实现 uart_ops->start_rx
 * 启动接收中断
 */
static void pl011_start_rx(struct uart_port *port)
{
    struct pl011_port *pl011 = to_pl011_port(port);
    pl011->regs->imsc |= PL011_IMSC_RXIM | PL011_IMSC_RTIM;
}

/**
 * 实现 uart_ops->stop_rx
 * 停止接收中断
 */
static void pl011_stop_rx(struct uart_port *port)
{
    struct pl011_port *pl011 = to_pl011_port(port);
    pl011->regs->imsc &= ~(PL011_IMSC_RXIM | PL011_IMSC_RTIM);
}

/**
 * PL011 硬件操作接口实现
 */
static const struct uart_ops pl011_uart_ops = {
    .set_termios = pl011_set_termios,
    .startup = pl011_startup,
    .shutdown = pl011_shutdown,
    .tx_char = pl011_tx_char,
    .rx_char = pl011_rx_char,
    .tx_ready = pl011_tx_ready,
    .tx_empty = pl011_tx_empty,
    .rx_ready = pl011_rx_ready,
    .start_tx = pl011_start_tx,
    .stop_tx = pl011_stop_tx,
    .start_rx = pl011_start_rx,
    .stop_rx = pl011_stop_rx,
};

/***************************************************************************
 * 驱动接口 (probe/remove)
 ***************************************************************************/

static int pl011_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct pl011_port *pl011;
    phys_addr_t reg_addr;
    size_t reg_size;
    int ret;

    /* 使用托管内存分配 pl011_port */
    pl011 = devm_kzalloc(dev, sizeof(*pl011), GFP_KERNEL);
    if (!pl011)
        return -ENOMEM;

    /* 初始化 uart_port */
    pl011->port.dev = dev;
    pl011->port.ops = &pl011_uart_ops;
    pl011->port.config.uartclk = 24000000;
    pl011->port.is_console = true;

#ifdef CONFIG_FT_E2000S_AARCH32
    pl011->port.config.uartclk = 100000000;
#endif

#if defined(CONFIG_QEMU_AARCH64) || defined(CONFIG_QEMU_AARCH32)
    pl011->sbsa = true;
#else
    pl011->sbsa = (bool)pdev->dev.driver->of_match_table->data;
#endif

    /* 设置 dev->priv 指向 uart_port */
    dev->priv = &pl011->port;

    /* 获取寄存器地址并映射 */
    ret = of_device_get_resource_reg(dev, &reg_addr, &reg_size);
    if (ret < 0)
    {
        KLOG_E("Failed to get register address");
        return ret;
    }

    KLOG_I("reg: 0x%" PRIx64 ", size: 0x%" PRIxPTR, reg_addr, reg_size);

    /* 使用托管 I/O 映射 */
    pl011->regs = (volatile struct pl011_regs *)devm_ioremap(dev, reg_addr, reg_size);
    if (!pl011->regs)
        return -ENOMEM;

    pl011->port.membase = (void *)pl011->regs;

    if (pl011->sbsa)
    {
        pl011->port.config.baudrate = 115200;
        pl011->port.config.data_bits = 8;
        pl011->port.config.stop_bit = 0;
        pl011->port.config.parity = 0;
    }
    else
    {
        pl011_init_config(&pl011->port);
    }

    /* 分配中断号 */
    pl011->port.irq = ttos_pic_irq_alloc(dev, 0);

    /* 使用托管中断注册（使用通用中断处理函数） */
    ret = devm_request_irq(dev, pl011->port.irq, uart_interrupt_handler, 0, "pl011", dev);
    if (ret)
    {
        KLOG_E("Failed to request IRQ %d, ret=%d", pl011->port.irq, ret);
        return ret;
    }

    /* 初始化时先 mask 中断，attach 时再 unmask */
    ttos_pic_irq_mask(pl011->port.irq);

    /* 使用统一的 UART 注册函数（代替 800 行重复代码！） */
    ret = uart_register_port(dev);
    if (ret < 0)
    {
        KLOG_E("Failed to register UART port");
        return ret;
    }

    KLOG_I("PL011 UART initialized successfully");
    return 0;
}

static void pl011_remove(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct pl011_port *pl011 = (struct pl011_port *)dev->priv;

    /* 停止硬件 */
    if (pl011 && !pl011->sbsa)
    {
        pl011->regs->cr &= ~PL011_CR_UARTEN;
    }

    /* 所有资源（内存、I/O映射、中断）由 devres 自动释放 */
    uart_unregister_port(dev);
}

static struct of_device_id pl011_table[] = {
    {.compatible = "arm,sbsa-uart", .data = (void *)true},
    {.compatible = "arm,pl011", .data = (void *)false},
    {/* end of list */},
};

static struct platform_driver pl011_driver = {
    .probe = pl011_probe,
    .remove = pl011_remove,
    .driver =
        {
            .name = "pl011",
            .of_match_table = &pl011_table[0],
        },
};

int pl011_init(void)
{
    return platform_add_driver(&pl011_driver);
}
INIT_EXPORT_DRIVER(pl011_init, "pl011 driver init");

#endif /* CONFIG_UART_PL011 */
