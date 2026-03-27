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

#ifndef __SERIAL_CORE_H__
#define __SERIAL_CORE_H__

#include <driver/device.h>
#include <stdbool.h>
#include <stdint.h>
#include <termios.h>

struct char_ops_s;
struct uart_port;
struct uart_ops;

/**
 * UART 配置参数
 */
struct uart_config
{
    uint32_t baudrate;  /* 波特率 */
    uint32_t uartclk;   /* UART 时钟频率 */
    uint8_t parity;     /* 0=无, 1=奇, 2=偶 */
    uint8_t stop_bit;   /* 0=1停止位, 1=2停止位 */
    uint8_t data_bits;  /* 数据位 (5/6/7/8) */
    uint8_t flow_ctrl;  /* 流控使能 */
    tcflag_t c_cflag;   /* c_cflag */
    speed_t __c_ispeed; /* c_ispeed */
    speed_t __c_ospeed; /* c_ospeed */
};

/**
 * UART 硬件操作接口
 *
 * 驱动只需实现这些最小化的硬件操作函数，
 * 通用逻辑由 uart_core 处理
 */
struct uart_ops
{
    /**
     * 设置 UART 参数（波特率、数据位等）
     * 必须实现
     */
    int (*set_termios)(struct uart_port *port);

    /**
     * 启动 UART（使能硬件）
     * 可选，某些 UART 不需要
     */
    int (*startup)(struct uart_port *port);

    /**
     * 停止 UART（禁用硬件）
     * 可选
     */
    void (*shutdown)(struct uart_port *port);

    /**
     * 发送单个字符
     * 必须实现
     */
    void (*tx_char)(struct uart_port *port, int ch);

    /**
     * 接收单个字符
     * 必须实现
     * @param status 返回接收状态（错误位等）
     */
    int (*rx_char)(struct uart_port *port, unsigned int *status);

    /**
     * 检查发送 FIFO 是否可写入
     * 必须实现
     */
    bool (*tx_ready)(struct uart_port *port);

    /**
     * 检查发送 FIFO 是否为空
     * 必须实现
     */
    bool (*tx_empty)(struct uart_port *port);

    /**
     * 检查接收 FIFO 是否有数据
     * 必须实现
     */
    bool (*rx_ready)(struct uart_port *port);

    /**
     * 启动发送中断
     * 必须实现
     */
    void (*start_tx)(struct uart_port *port);

    /**
     * 停止发送中断
     * 必须实现
     */
    void (*stop_tx)(struct uart_port *port);

    /**
     * 启动接收中断
     * 必须实现
     */
    void (*start_rx)(struct uart_port *port);

    /**
     * 停止接收中断
     * 必须实现
     */
    void (*stop_rx)(struct uart_port *port);
};

/**
 * UART 端口抽象
 *
 * 所有 UART 驱动共用这个结构体
 */
struct uart_port
{
    struct device *dev;         /* 关联的设备 */
    struct uart_config config;  /* UART 配置参数 */
    const struct uart_ops *ops; /* 硬件操作函数 */

    /* 硬件资源 */
    uint64_t io_base;  /* IO 端口基址 (x86) */
    void *membase;     /* 内存映射基址 (ARM/RISC-V) */
    uint32_t irq;      /* 中断号 */
    uint32_t hw_irq;   /* 硬件中断号 */
    uint32_t fifosize; /* FIFO 大小 */
    uint8_t io_type;   /* IO 访问类型 */

#define UART_IO_PORT 0  /* 8b I/O port access */
#define UART_IO_MEM8 1  /* 16b little endian */
#define UART_IO_MEM16 2 /* 16b little endian */
#define UART_IO_MEM32 3 /* 32b little endian */

    /* IO 访问函数（可选，某些驱动需要自定义） */
    uint32_t (*serial_in)(struct uart_port *, uint32_t);
    void (*serial_out)(struct uart_port *, uint32_t, uint32_t);

    /* 波特率分频器（可选，某些驱动需要） */
    uint32_t (*get_divisor)(struct uart_port *);
    void (*set_divisor)(struct uart_port *, uint32_t div);

    /* 驱动私有数据 */
    void *private_data;

    /* 标志位 */
    bool is_console; /* 是否为控制台 */
};

/* IO 访问辅助函数 */
static inline uint32_t serialin(struct uart_port *up, int offset)
{
    return up->serial_in(up, offset);
}

static inline void serialout(struct uart_port *up, int offset, int value)
{
    return up->serial_out(up, offset, value);
}

/****************************************************************************
 * 公共 API - 供驱动使用
 ****************************************************************************/

/**
 * @brief 注册 UART 端口
 *
 * 驱动在 probe 函数中调用此函数完成 UART 注册：
 * 1. 初始化 char_class_type 和缓冲区
 * 2. 设置统一的 char_ops_s
 * 3. 注册为字符设备 (/dev/ttySx)
 *
 * @param dev 设备指针，dev->priv 必须指向 struct uart_port
 * @return 0 成功，负值失败
 */
int uart_register_port(struct device *dev);

/**
 * @brief 注销 UART 端口
 *
 * 驱动在 remove 函数中调用（可选）
 * 实际上 devres 会自动清理，但保留此函数以保持对称性
 *
 * @param dev 设备指针
 */
void uart_unregister_port(struct device *dev);

/**
 * @brief 通用 UART 中断处理函数
 *
 * 大多数 UART 中断逻辑相同：
 * - 检查 rx_ready -> char_recvchars()
 * - 检查 tx_ready -> char_xmitchars()
 *
 * 驱动可以在 devm_request_irq 中直接使用此函数，
 * 或者自己实现复杂的中断处理
 *
 * @param irq 中断号
 * @param param 设备指针
 */
void uart_interrupt_handler(u32 irq, void *param);

/**
 * @brief 根据IO类型设置串口使用的IO函数
 *
 * @param p 串口端口
 * @return int 0 成功，负值失败
 */
int uart_set_io_from_type(struct uart_port *p);

#endif /* __SERIAL_CORE_H__ */