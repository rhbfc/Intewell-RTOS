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

#ifndef __SERIAL_8250_H__
#define __SERIAL_8250_H__

#include "serial_core.h"
// #include <driver/clk.h>
#include <stdint.h>

struct uart_8250_port;

struct uart_8250_port
{
    struct uart_port port;
    uint8_t ier;
    bool use_fifo;
    /* 8250系列串口外设的RBR和HTR寄存器可映射为不同功能，必须保证串行化处理，避免错误访问 */
    ttos_spinlock_t hw_lock;
#ifdef CONFIG_CLK
    struct clk mclk; /* UART clock descriptor */
#endif
};

static inline struct uart_8250_port *to_uart_8250_port(struct uart_port *up)
{
    return container_of(up, struct uart_8250_port, port);
}

/**
 * @brief 8250 通用的 uart_ops 实现
 *
 * DW8250、i8250 等驱动可以直接使用这个 ops，
 * 只需设置正确的 serial_in/serial_out 函数
 */
extern const struct uart_ops uart_8250_ops;

/**
 * @brief 8250 专用中断处理函数
 *
 * 比通用的 uart_interrupt_handler 更复杂，
 * 因为 8250 需要循环读取 IIR 寄存器
 */
void uart_8250_interrupt(uint32_t irq, void *arg);

int uart8250_init_config(struct uart_port *port);

#endif /* __SERIAL_8250_H__ */