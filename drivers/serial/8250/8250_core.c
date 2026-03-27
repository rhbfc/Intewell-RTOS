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
 * @file    drivers/serial/8250/8250_core.c
 * @brief   8250 系列 UART 通用端口操作
 * @author  TTOS Driver Team
 * @date    2025-10-10
 *
 * 提供 8250 系列 UART 的通用 uart_ops 实现
 * DW8250、i8250 等驱动可以直接使用
 */

#include <driver/class/chardev.h>
#include <driver/serial/serial_8250.h>
#include <driver/serial/serial_core.h>
#include <driver/serial/serial_reg.h>
#include <ttos_pic.h>

#undef KLOG_TAG
#define KLOG_TAG "8250_port"
#include <klog.h>

static bool uart_8250_tx_empty(struct uart_port *port);

/****************************************************************************
 * 8250 通用辅助函数
 ****************************************************************************/

/**
 * 计算波特率分频器
 */
static inline uint32_t uart_8250_get_divisor(struct uart_port *port)
{
    return (port->config.uartclk + (port->config.baudrate << 3)) / (port->config.baudrate << 4);
}

/**
 * 设置波特率分频器
 */
static inline void uart_8250_set_divisor(struct uart_port *port, uint32_t div)
{
    serialout(port, UART_DLM_OFFSET, div >> 8);
    serialout(port, UART_DLL_OFFSET, div & 0xff);
}

/**
 * 禁用所有中断
 */
static void uart_8250_disable_int(struct uart_8250_port *up)
{
    up->ier = 0;
    serialout(&up->port, UART_IER_OFFSET, 0);
}

/****************************************************************************
 * 8250 uart_ops 实现
 ****************************************************************************/

int uart8250_init_config(struct uart_port *port)
{
    struct uart_8250_port *up = to_uart_8250_port(port);

    uint32_t lcr = serialin(port, UART_LCR_OFFSET);
    uint32_t div;
    uint32_t dll, dlm;
    uint32_t baudrate;
    uint32_t uartclk = port->config.uartclk;

    spin_lock_init(&up->hw_lock);

    serialout(port, UART_LCR_OFFSET, lcr | UART_LCR_DLAB);

    dll = serialin(port, UART_DLL_OFFSET);
    dlm = serialin(port, UART_DLM_OFFSET);
    div = (dlm << 8) | dll;
    if (div == 0)
    {
        baudrate = CONFIG_UART_BAUD;
    }
    else
    {
        baudrate = uartclk / (div << 4);
    }

    serialout(port, UART_LCR_OFFSET, lcr);

    port->config.baudrate = baudrate;

    port->config.data_bits = ((lcr & UART_LCR_WLS_MASK) >> UART_LCR_WLS_SHIFT) + 5;

    port->config.stop_bit = !!(lcr & UART_LCR_STB);

    port->config.parity = (lcr & UART_LCR_PEN) ? ((lcr & UART_LCR_EPS) ? 2 : 1) : 0;

    return 0;
}

/**
 * 发送单个字符
 */
static void uart_8250_tx_char(struct uart_port *port, int ch)
{
    struct uart_8250_port *up = to_uart_8250_port(port);
    long flags = 0;

    spin_lock_irqsave(&up->hw_lock, flags);
    serialout(port, UART_THR_OFFSET, ch);
    spin_unlock_irqrestore(&up->hw_lock, flags);
}

/**
 * 接收单个字符
 */
static int uart_8250_rx_char(struct uart_port *port, unsigned int *status)
{
    uint32_t ch;
    long flags = 0;
    struct uart_8250_port *up = to_uart_8250_port(port);

    spin_lock_irqsave(&up->hw_lock, flags);
    *status = serialin(port, UART_LSR_OFFSET);
    ch = serialin(port, UART_RBR_OFFSET);
    spin_unlock_irqrestore(&up->hw_lock, flags);
    return ch;
}

/**
 * 检查发送 FIFO 是否可写
 */
static bool uart_8250_tx_ready(struct uart_port *port)
{
    uint8_t lsr = serialin(port, UART_LSR_OFFSET);
    return (lsr & UART_LSR_THRE) != 0;
}

/**
 * 检查发送 FIFO 是否为空
 */
static bool uart_8250_tx_empty(struct uart_port *port)
{
    return (serialin(port, UART_LSR_OFFSET) & UART_LSR_TEMT) != 0;
}

/**
 * 检查接收 FIFO 是否有数据
 */
static bool uart_8250_rx_ready(struct uart_port *port)
{
    return (serialin(port, UART_LSR_OFFSET) & UART_LSR_DR) != 0;
}

/**
 * 启动发送中断
 */
static void uart_8250_start_tx(struct uart_port *port)
{
    struct uart_8250_port *up = to_uart_8250_port(port);
    up->ier |= UART_IER_ETBEI;
    serialout(port, UART_IER_OFFSET, up->ier);
}

/**
 * 停止发送中断
 */
static void uart_8250_stop_tx(struct uart_port *port)
{
    struct uart_8250_port *up = to_uart_8250_port(port);
    up->ier &= ~UART_IER_ETBEI;
    serialout(port, UART_IER_OFFSET, up->ier);
}

/**
 * 启动接收中断
 */
static void uart_8250_start_rx(struct uart_port *port)
{
    struct uart_8250_port *up = to_uart_8250_port(port);
    up->ier |= UART_IER_ERBFI;
    serialout(port, UART_IER_OFFSET, up->ier);
}

/**
 * 停止接收中断
 */
static void uart_8250_stop_rx(struct uart_port *port)
{
    struct uart_8250_port *up = to_uart_8250_port(port);
    up->ier &= ~UART_IER_ERBFI;
    serialout(port, UART_IER_OFFSET, up->ier);
}

/**
 * 停止 UART
 */
static void uart_8250_shutdown(struct uart_port *port)
{
    struct uart_8250_port *up = to_uart_8250_port(port);
    uart_8250_disable_int(up);
}

/**
 * 设置 UART 参数
 */
static int uart_8250_set_termios(struct uart_port *port)
{
    struct uart_8250_port *up = to_uart_8250_port(port);
    long flags = 0;
    uint32_t lcr = 0;
    uint32_t div;

    /* 读取当前 IER */
    up->ier = serialin(port, UART_IER_OFFSET);

    /* 配置数据位 */
    switch (port->config.data_bits)
    {
    case 5:
        lcr |= UART_LCR_WLS_5BIT;
        break;
    case 6:
        lcr |= UART_LCR_WLS_6BIT;
        break;
    case 7:
        lcr |= UART_LCR_WLS_7BIT;
        break;
    case 8:
    default:
        lcr |= UART_LCR_WLS_8BIT;
        break;
    }

    /* 停止位 */
    if (port->config.stop_bit)
        lcr |= UART_LCR_STB;

    /* 校验位 */
    if (port->config.parity == 1)
        lcr |= UART_LCR_PEN; /* 奇校验 */
    else if (port->config.parity == 2)
        lcr |= UART_LCR_PEN | UART_LCR_EPS; /* 偶校验 */

    /* DW8250的LCR寄存器仅在空闲状态支持写操作,
     * 为了最大限度减少持锁时间(波特率越低,FIFO越深,时间越长),此处屏蔽串口中断并等待FIFO和发送保持寄存器为空
     */
    ttos_pic_irq_mask(port->irq);
    while (!uart_8250_tx_empty(port))
        ;

    spin_lock_irqsave(&up->hw_lock, flags);

    /* 设置波特率：进入 DLAB 模式 */
    serialout(port, UART_LCR_OFFSET, lcr | UART_LCR_DLAB);

    div = uart_8250_get_divisor(port);
    uart_8250_set_divisor(port, div);

    /* 清除 DLAB */
    serialout(port, UART_LCR_OFFSET, lcr);

    /* 配置 FIFO */
    if (up->use_fifo)
    {
        serialout(port, UART_FCR_OFFSET,
                  UART_FCR_RXTRIGGER_8 | UART_FCR_TXRST | UART_FCR_RXRST | UART_FCR_FIFOEN);
    }

    spin_unlock_irqrestore(&up->hw_lock, flags);

    /* 解除中断屏蔽 */
    ttos_pic_irq_unmask(port->irq);

    return 0;
}

/**
 * 8250 通用 uart_ops
 */
const struct uart_ops uart_8250_ops = {
    .set_termios = uart_8250_set_termios,
    .shutdown = uart_8250_shutdown,
    .tx_char = uart_8250_tx_char,
    .rx_char = uart_8250_rx_char,
    .tx_ready = uart_8250_tx_ready,
    .tx_empty = uart_8250_tx_empty,
    .rx_ready = uart_8250_rx_ready,
    .start_tx = uart_8250_start_tx,
    .stop_tx = uart_8250_stop_tx,
    .start_rx = uart_8250_start_rx,
    .stop_rx = uart_8250_stop_rx,
};

/****************************************************************************
 * 8250 专用中断处理（比通用的稍微复杂一点）
 ****************************************************************************/

void uart_8250_interrupt(uint32_t irq, void *arg)
{
    struct device *dev = (struct device *)arg;
    struct uart_port *port;
    uint32_t iir;
    int passes = 0;

    if (!dev || !dev->priv)
        return;

    port = (struct uart_port *)dev->priv;

    /* 8250 需要循环处理多个中断源 */
    while (passes < 256)
    {
        iir = serialin(port, UART_IIR_OFFSET);

        /* No interrupt pending */
        if (iir & UART_IIR_INTSTATUS)
            break;

        switch (iir & UART_IIR_INTID_MASK)
        {
        case UART_IIR_INTID_THRE: /* THR empty */
            char_xmitchars(dev);
            break;
        case UART_IIR_INTID_RDA: /* Received data available */
        case UART_IIR_INTID_CTI: /* Received data timeout */
            char_recvchars(dev);
            break;
        case UART_IIR_INTID_MSI:
            serialin(port, UART_MSR_OFFSET);
            break;
        case UART_IIR_INTID_RLS:
            serialin(port, UART_LSR_OFFSET);
            break;
        case 7:
            serialin(port, UART_USR_OFFSET);
            break;
        default:
            break;
        }

        passes++;
    }
}
